// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This utility program exists to process the False Start blacklist file into
// a static hash table so that it can be efficiently queried by Chrome.

#include <algorithm>
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "net/base/ssl_false_start_blacklist.h"

typedef std::vector<std::string> Hosts;

// Parses |input| as a blacklist data file, and returns the set of hosts it
// contains.
Hosts ParseHosts(const std::string& input) {
  Hosts hosts;
  size_t line_start = 0;
  bool is_comment = false;
  bool non_whitespace_seen = false;
  for (size_t i = 0; i <= input.size(); ++i) {
    if (i == input.size() || input[i] == '\n') {
      if (!is_comment && non_whitespace_seen) {
        size_t len = i - line_start;
        if (i > 0 && input[i - 1] == '\r')
          len--;
        hosts.push_back(input.substr(line_start, len));
      }
      is_comment = false;
      non_whitespace_seen = false;
      line_start = i + 1;
    } else if (input[i] != ' ' && input[i] != '\t' && input[i] != '\r') {
      non_whitespace_seen = true;
      if (i == line_start && input[i] == '#')
        is_comment = true;
    }
  }
  VLOG(1) << "Have " << hosts.size() << " hosts after parse";
  return hosts;
}

// Returns |host| with any initial "www." and trailing dots removed.  Partly
// based on net::StripWWW().
std::string StripWWWAndTrailingDots(const std::string& host) {
  const std::string www("www.");
  const size_t start = StartsWithASCII(host, www, true) ? www.length() : 0;
  const size_t end = host.find_last_not_of('.');
  return (end == std::string::npos) ?
      std::string() : host.substr(start, end - start + 1);
}

// Removes all duplicates from |hosts|.
static void RemoveDuplicateEntries(std::vector<std::string>* hosts) {
  std::sort(hosts->begin(), hosts->end());
  hosts->erase(std::unique(hosts->begin(), hosts->end()), hosts->end());
  VLOG(1) << "Have " << hosts->size() << " hosts after removing duplicates";
}

// Returns the parent domain for |host|, or the empty string if the name is a
// top-level domain.
static std::string ParentDomain(const std::string& host) {
  const size_t first_dot = host.find('.');
  return (first_dot == std::string::npos) ?
      std::string() : host.substr(first_dot + 1);
}

// Predicate which returns true when a hostname has a parent domain in the set
// of hosts provided at construction time.
class ParentInSet : public std::unary_function<std::string, bool> {
 public:
  explicit ParentInSet(const std::set<std::string>& hosts) : hosts_(hosts) {}

  bool operator()(const std::string& host) const {
    for (std::string parent(ParentDomain(host)); !parent.empty();
         parent = ParentDomain(parent)) {
      if (hosts_.count(parent)) {
        VLOG(1) << "Removing " << host << " as redundant";
        return true;
      }
    }
    return false;
  }

 private:
  const std::set<std::string>& hosts_;
};

// Removes any hosts which are subdomains of other hosts.  E.g.
// "foo.example.com" would be removed if "example.com" were also included.
static void RemoveRedundantEntries(Hosts* hosts) {
  std::set<std::string> hosts_set;
  for (Hosts::const_iterator i(hosts->begin()); i != hosts->end(); ++i)
    hosts_set.insert(*i);
  hosts->erase(std::remove_if(hosts->begin(), hosts->end(),
                              ParentInSet(hosts_set)), hosts->end());
  VLOG(1) << "Have " << hosts->size() << " hosts after removing redundants";
}

// Returns true iff all |hosts| are less than 256 bytes long (not including the
// terminating NUL) and contain two or more dot-separated components.
static bool CheckLengths(const Hosts& hosts) {
  for (Hosts::const_iterator i(hosts.begin()); i != hosts.end(); ++i) {
     if (i->size() >= 256) {
       fprintf(stderr, "Entry '%s' is too large\n", i->c_str());
       return false;
     }
     if (net::SSLFalseStartBlacklist::LastTwoComponents(*i).empty()) {
       fprintf(stderr, "Entry '%s' contains too few labels\n", i->c_str());
       return false;
     }
  }

  return true;
}

// Returns the contents of the output file to be written.
std::string GenerateOutput(const Hosts& hosts) {
  // Hash each host into its appropriate bucket.
  VLOG(1) << "Using " << net::SSLFalseStartBlacklist::kBuckets
          << " entry hash table";
  Hosts buckets[net::SSLFalseStartBlacklist::kBuckets];
  for (Hosts::const_iterator i(hosts.begin()); i != hosts.end(); ++i) {
    const uint32 hash = net::SSLFalseStartBlacklist::Hash(
        net::SSLFalseStartBlacklist::LastTwoComponents(*i));
    buckets[hash & (net::SSLFalseStartBlacklist::kBuckets - 1)].push_back(*i);
  }

  // Write header.
  std::ostringstream output;
  output << "// Copyright (c) 2011 The Chromium Authors. All rights reserved.\n"
            "// Use of this source code is governed by a BSD-style license that"
            " can be\n// found in the LICENSE file.\n\n// WARNING: This code is"
            " generated by ssl_false_start_blacklist_process.cc.\n// Do not "
            "edit.\n\n#include \"net/base/ssl_false_start_blacklist.h\"\n\n"
            "namespace net {\n\nconst uint32 "
            "SSLFalseStartBlacklist::kHashTable["
         << net::SSLFalseStartBlacklist::kBuckets << " + 1] = {\n  0,\n";

  // Construct data table, writing out the size as each bucket is appended.
  std::string table_data;
  size_t max_bucket_size = 0;
  for (size_t i = 0; i < net::SSLFalseStartBlacklist::kBuckets; i++) {
    max_bucket_size = std::max(max_bucket_size, buckets[i].size());
    for (Hosts::const_iterator j(buckets[i].begin()); j != buckets[i].end();
         ++j) {
      table_data.push_back(static_cast<char>(j->size()));
      table_data.append(*j);
    }
    output << "  " << table_data.size() << ",\n";
  }
  output << "};\n\n";
  VLOG(1) << "Largest bucket has " << max_bucket_size << " entries";

  // Write data table, breaking lines after 72+ (2 indent, 70+ data) characters.
  output << "const char SSLFalseStartBlacklist::kHashData[] = {\n";
  for (size_t i = 0, line_length = 0; i < table_data.size(); i++) {
    if (line_length == 0)
      output << "  ";
    std::ostringstream::pos_type current_length = output.tellp();
    output << static_cast<int>(table_data[i]) << ", ";
    line_length += output.tellp() - current_length;
    if (i == table_data.size() - 1) {
      output << "\n};\n";
    } else if (line_length >= 70) {
      output << "\n";
      line_length = 0;
    }
  }
  output << "\n}  // namespace net\n";
  return output.str();
}

#if defined(OS_WIN)
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
#elif defined(OS_POSIX)
int main(int argc, char* argv[], char* envp[]) {
#endif
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <blacklist file> <output .c file>\n", argv[0]);
    return 1;
  }

  // Read input file.
  std::string input;
  if (!file_util::ReadFileToString(FilePath(argv[1]), &input)) {
    fprintf(stderr, "Failed to read input file '%s'\n", argv[1]);
    return 2;
  }
  Hosts hosts(ParseHosts(input));

  // Sanitize |hosts|.
  std::transform(hosts.begin(), hosts.end(), hosts.begin(),
                 StripWWWAndTrailingDots);
  RemoveDuplicateEntries(&hosts);
  RemoveRedundantEntries(&hosts);
  if (!CheckLengths(hosts))
    return 3;

  // Write output file.
  const std::string output_str(GenerateOutput(hosts));
  if (file_util::WriteFile(FilePath(argv[2]), output_str.data(),
      output_str.size()) == static_cast<int>(output_str.size()))
    return 0;
  fprintf(stderr, "Failed to write output file '%s'\n", argv[2]);
  return 4;
}
