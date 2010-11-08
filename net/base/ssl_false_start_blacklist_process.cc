// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This utility program exists to process the False Start blacklist file into
// a static hash table so that it can be efficiently queried by Chrome.

#include <stdio.h>
#include <stdlib.h>

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/base/ssl_false_start_blacklist.h"

using net::SSLFalseStartBlacklist;

static const unsigned kBuckets = SSLFalseStartBlacklist::kBuckets;

static bool verbose = false;

static int
usage(const char* argv0) {
  fprintf(stderr, "Usage: %s <blacklist file> <output .c file>\n", argv0);
  return 1;
}

// StripWWWPrefix removes "www." from the beginning of any elements of the
// vector.
static void StripWWWPrefix(std::vector<std::string>* hosts) {
  static const char kPrefix[] = "www.";
  static const unsigned kPrefixLen = sizeof(kPrefix) - 1;

  for (size_t i = 0; i < hosts->size(); i++) {
    const std::string& h = (*hosts)[i];
    if (h.size() >= kPrefixLen &&
        memcmp(h.data(), kPrefix, kPrefixLen) == 0) {
      (*hosts)[i] = h.substr(kPrefixLen, h.size() - kPrefixLen);
    }
  }
}

// RemoveDuplicateEntries removes all duplicates from |hosts|.
static void RemoveDuplicateEntries(std::vector<std::string>* hosts) {
  std::set<std::string> hosts_set;
  std::vector<std::string> ret;

  for (std::vector<std::string>::const_iterator
       i = hosts->begin(); i != hosts->end(); i++) {
    if (hosts_set.count(*i)) {
      if (verbose)
        fprintf(stderr, "Removing duplicate entry for %s\n", i->c_str());
      continue;
    }
    hosts_set.insert(*i);
    ret.push_back(*i);
  }

  hosts->swap(ret);
}

// ParentDomain returns the parent domain for a given domain name or the empty
// string if the name is a top-level domain.
static std::string ParentDomain(const std::string& in) {
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == '.') {
      return in.substr(i + 1, in.size() - i - 1);
    }
  }

  return std::string();
}

// RemoveRedundantEntries removes any entries which are subdomains of other
// entries. (i.e. foo.example.com would be removed if example.com were also
// included.)
static void RemoveRedundantEntries(std::vector<std::string>* hosts) {
  std::set<std::string> hosts_set;
  std::vector<std::string> ret;

  for (std::vector<std::string>::const_iterator
       i = hosts->begin(); i != hosts->end(); i++) {
    hosts_set.insert(*i);
  }

  for (std::vector<std::string>::const_iterator
       i = hosts->begin(); i != hosts->end(); i++) {
    std::string parent = ParentDomain(*i);
    while (!parent.empty()) {
      if (hosts_set.count(parent))
        break;
      parent = ParentDomain(parent);
    }
    if (parent.empty()) {
      ret.push_back(*i);
    } else {
      if (verbose)
        fprintf(stderr, "Removing %s as redundant\n", i->c_str());
    }
  }

  hosts->swap(ret);
}

// CheckLengths returns true iff every host is less than 256 bytes long (not
// including the terminating NUL) and contains two or more labels.
static bool CheckLengths(const std::vector<std::string>& hosts) {
  for (std::vector<std::string>::const_iterator
       i = hosts.begin(); i != hosts.end(); i++) {
     if (i->size() >= 256) {
       fprintf(stderr, "Entry %s is too large\n", i->c_str());
       return false;
     }
     if (SSLFalseStartBlacklist::LastTwoLabels(i->c_str()) == NULL) {
       fprintf(stderr, "Entry %s contains too few labels\n", i->c_str());
       return false;
     }
  }

  return true;
}

int main(int argc, char** argv) {
  if (argc != 3)
    return usage(argv[0]);

  const char* input_file = argv[1];
  const char* output_file = argv[2];
  FILE* input = fopen(input_file, "rb");
  if (!input) {
    perror("open");
    return usage(argv[0]);
  }

  if (fseek(input, 0, SEEK_END)) {
    perror("fseek");
    return 1;
  }

  const long input_size = ftell(input);

  if (fseek(input, 0, SEEK_SET)) {
    perror("fseek");
    return 1;
  }

  char* buffer = static_cast<char*>(malloc(input_size));
  long done = 0;
  while (done < input_size) {
    size_t n = fread(buffer + done, 1, input_size - done, input);
    if (n == 0) {
      perror("fread");
      free(buffer);
      fclose(input);
      return 1;
    }
    done += n;
  }
  fclose(input);

  std::vector<std::string> hosts;

  off_t line_start = 0;
  bool is_comment = false;
  bool non_whitespace_seen = false;
  for (long i = 0; i <= input_size; i++) {
    if (i == input_size || buffer[i] == '\n') {
      if (!is_comment && non_whitespace_seen) {
        long len = i - line_start;
        if (i > 0 && buffer[i-1] == '\r')
          len--;
        hosts.push_back(std::string(&buffer[line_start], len));
      }
      is_comment = false;
      non_whitespace_seen = false;
      line_start = i + 1;
      continue;
    }

    if (i == line_start && buffer[i] == '#')
      is_comment = true;
    if (buffer[i] != ' ' && buffer[i] != '\t' && buffer[i] != '\r')
      non_whitespace_seen = true;
  }
  free(buffer);

  fprintf(stderr, "Have %d hosts after parse\n", (int) hosts.size());
  StripWWWPrefix(&hosts);
  RemoveDuplicateEntries(&hosts);
  fprintf(stderr, "Have %d hosts after removing duplicates\n", (int) hosts.size());
  RemoveRedundantEntries(&hosts);
  fprintf(stderr, "Have %d hosts after removing redundants\n", (int) hosts.size());
  if (!CheckLengths(hosts)) {
    fprintf(stderr, "One or more entries is too large or too small\n");
    return 2;
  }

  fprintf(stderr, "Using %d entry hash table\n", kBuckets);
  uint32 table[kBuckets];
  std::vector<std::string> buckets[kBuckets];

  for (std::vector<std::string>::const_iterator
       i = hosts.begin(); i != hosts.end(); i++) {
    const char* last_two_labels =
        SSLFalseStartBlacklist::LastTwoLabels(i->c_str());
    const unsigned h = SSLFalseStartBlacklist::Hash(last_two_labels);
    buckets[h & (kBuckets - 1)].push_back(*i);
  }

  std::string table_data;
  unsigned max_bucket_size = 0;
  for (unsigned i = 0; i < kBuckets; i++) {
    if (buckets[i].size() > max_bucket_size)
      max_bucket_size = buckets[i].size();

    table[i] = table_data.size();
    for (std::vector<std::string>::const_iterator
         j = buckets[i].begin(); j != buckets[i].end(); j++) {
      table_data.push_back((char) j->size());
      table_data.append(*j);
    }
  }

  fprintf(stderr, "Largest bucket has %d entries\n", max_bucket_size);

  FILE* out = fopen(output_file, "w+");
  if (!out) {
    perror("opening output file");
    return 4;
  }

  fprintf(out, "// Copyright (c) 2010 The Chromium Authors. All rights "
          "reserved.\n// Use of this source code is governed by a BSD-style "
          "license that can be\n// found in the LICENSE file.\n\n");
  fprintf(out, "// WARNING: this code is generated by\n"
               "// ssl_false_start_blacklist_process.cc. Do not edit.\n\n");
  fprintf(out, "#include \"base/basictypes.h\"\n\n");
  fprintf(out, "#include \"net/base/ssl_false_start_blacklist.h\"\n\n");
  fprintf(out, "namespace net {\n\n");
  fprintf(out, "const uint32 SSLFalseStartBlacklist::kHashTable[%d + 1] = {\n",
          kBuckets);
  for (unsigned i = 0; i < kBuckets; i++) {
    fprintf(out, "  %u,\n", (unsigned) table[i]);
  }
  fprintf(out, "  %u,\n", (unsigned) table_data.size());
  fprintf(out, "};\n\n");

  fprintf(out, "const char SSLFalseStartBlacklist::kHashData[] = {\n");
  for (unsigned i = 0, line_length = 0; i < table_data.size(); i++) {
    if (line_length == 0)
      fprintf(out, "  ");
    uint8 c = static_cast<uint8>(table_data[i]);
    line_length += fprintf(out, "%d, ", c);
    if (i == table_data.size() - 1) {
      fprintf(out, "\n};\n");
    } else if (line_length >= 70) {
      fprintf(out, "\n");
      line_length = 0;
    }
  }
  fprintf(out, "\n}  // namespace net\n");
  fclose(out);

  return 0;
}
