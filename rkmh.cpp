#include <iostream>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <string>
#include <zlib.h>
#include <omp.h>
#include <getopt.h>
#include <map>
#include "mkmh.hpp"
#include "kseq.hpp"
#include "equiv.hpp"

KSEQ_INIT(gzFile, gzread)

    using namespace std;
    using namespace mkmh;
    void print_help(char** argv){
        cerr << "Usage: " << argv[0] << " [options]" << endl
            << "Options:" << endl
            << "--reads/-r   <READFILE>" << endl
            << "--fasta/-f   <FASTAFILE>" << endl
            << "--kmer/-k    <KMERSIZE>" << endl
            << "--minhash/-h <HASHSIZE>" << endl
            << "--threads/-t <THREADS>" << endl;
    }


int main(int argc, char** argv){
    char* ref_file;
    char* read_file;
    vector<int> kmer;
    int sketch_size = -1;
    int threads = 1;

    int c;
    if (argc < 2){
        print_help(argv);
        exit(1);
    }

    while (true){
        static struct option long_options[] =
        {
            {"help", no_argument, 0, 'h'},
            {"kmer", no_argument, 0, 'k'},
            {"reads", required_argument, 0, 'r'},
            {"fasta", required_argument, 0, 'f'},
            {"minhash", required_argument, 0, 'm'},
            {"threads", required_argument, 0, 't'},
            {0,0,0,0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "hm:k:r:f:t:", long_options, &option_index);
        if (c == -1){
            break;
        }

        switch (c){
            case 't':
                threads = atoi(optarg);
                break;
            case 'f':
                ref_file = optarg;
                break;
            case 'r':
                read_file = optarg;
                break;
            case 'k':
                kmer.push_back(atoi(optarg));
                break;
            case '?':
            case 'h':
                print_help(argv);
                exit(1);
                break;
            case 'm':
                sketch_size = atoi(optarg);
                break;
            default:
                print_help(argv);
                abort();

        }
    }


    map<string, string> ref_to_seq;
    map<string, vector<int64_t> > ref_to_hashes;
    map<string, vector<string> > ref_to_kmers;

    map<string, string> read_to_seq;
    map<string, vector<int64_t> > read_to_hashes;
    map<string, vector<string> > read_to_kmers;

    omp_set_num_threads(threads);
    // Read in fastas
    gzFile fp;
    kseq_t *seq;
    int l;

    if (!(strlen(ref_file) == 0)){
        fp = gzopen(ref_file, "r");
        seq = kseq_init(fp);
        // Read in reads, cluster, spit it back out
        while ((l = kseq_read(seq)) >= 0) {
            ref_to_seq[seq->name.s] = to_upper(seq->seq.s);
        } 

        cerr << "Loaded " << ref_to_seq.size() << " sequences." << endl;
    }
    else{
        cerr << "Please provide a fasta file containing references." << endl;
        exit(1);
    }

    if (!(strlen(read_file) == 0)){
        fp = gzopen(read_file, "r");
        seq = kseq_init(fp);
        // Read in reads, cluster, spit it back out
        while ((l = kseq_read(seq)) >= 0) {
            read_to_seq[seq->name.s] = to_upper(seq->seq.s);
        }

        cerr << "Loaded " << read_to_seq.size() << " sequences." << endl;
    }
    else{
        cerr << "Please provide a read file containing query sequences." << endl;
        exit(1);
    }


    if (sketch_size > 0){
        cerr << "Making reference sketches..." << endl;
        map<string, string>::iterator itersk;
        for (itersk = ref_to_seq.begin(); itersk != ref_to_seq.end(); itersk++){
            ref_to_hashes[itersk->first] = minhash_64(itersk->second, kmer, sketch_size, true);
            //cerr << multi_kmerize(itersk->second, kmer)[0];
            //cout << ref_to_hashes[itersk->first][1] << endl;
        }

        cerr << "Processed " << ref_to_hashes.size() << " references to MinHashes" << endl;

        for (itersk = read_to_seq.begin(); itersk != read_to_seq.end(); itersk++){
            read_to_hashes[itersk->first] = minhash_64(itersk->second, kmer, sketch_size, true);

            tuple<string, int, int> result = classify_and_count(read_to_hashes[itersk->first], ref_to_hashes);
            cout  << "Sample: " << itersk->first << "\t"
                << "Result: " << std::get<0>(result) << "\t" << std::get<1>(result) << "\t" << std::get<2>(result) << endl;

            //string result = classify(read_to_hashes[itersk->first], ref_to_hashes);
            //cerr << result << endl;
            //cerr << multi_kmerize(itersk->second, kmer)[0];
            //cout << read_to_hashes[itersk->first][1] << endl;
        }

        cerr << "Processed " << read_to_hashes.size() << " reads to MinHashes" << endl;
        
        //TODO remove me!!

        map<string, vector<int64_t> >::iterator mitersk;
        int count = 0;
/*
        for (mitersk = read_to_hashes.begin(); mitersk != read_to_hashes.end(); mitersk++){
            //struct Classification result = classify_and_count(mitersk->second, sample_to_hashes);
            tuple<string, int, int> result = classify_and_count(mitersk->second, ref_to_hashes);
            //string result = classify(mitersk->second, ref_to_hashes); 
            //cerr << result << endl;
            cerr  << "Sample: " << mitersk->first << "\t"
                << "Result: " << std::get<0>(result) << "\t" << std::get<1>(result) << "\t" << std::get<2>(result) << endl;

            count++;

            cerr << "Processed: " << count << " samples." << endl;
        }
*/
    }

    else{
        cerr << "Performing direct kmer-based comparison." << endl;
        map<string, string>::iterator itersk;
        for (itersk = ref_to_seq.begin(); itersk != ref_to_seq.end(); itersk++){
            ref_to_kmers[itersk->first] = multi_kmerize(itersk->second, kmer);

            std::sort(ref_to_kmers[itersk->first].begin(), ref_to_kmers[itersk->first].end());
        }

        cerr << "Processed " << ref_to_kmers.size() << " references to kmers." << endl;

        for (itersk = read_to_seq.begin(); itersk != read_to_seq.end(); itersk++){
            read_to_kmers[itersk->first] = multi_kmerize(itersk->second, kmer);
            std::sort(read_to_kmers[itersk->first].begin(), read_to_kmers[itersk->first].end());

            tuple<string, int, int> result = kmer_classify(read_to_kmers[itersk->first], ref_to_kmers);
            cout  << "Sample: " << itersk->first << "\t"
                << "Result: " << std::get<0>(result) << "\t" << std::get<1>(result) << "\t" << std::get<2>(result) << endl;


        }

        cerr << "Processed " << read_to_kmers.size() << " reads to kmers." << endl;


    }








    return 1;
}