// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"

class FilePath;

namespace chromeos {
namespace system {

// The parser is used to get machine info as name-value pairs. Defined
// here to be accessable by tests.
class NameValuePairsParser {
 public:
  typedef std::map<std::string, std::string> NameValueMap;

  // The obtained info will be written into machine_info.
  explicit NameValuePairsParser(NameValueMap* map);

  void AddNameValuePair(const std::string& key, const std::string& value);

  // Executes tool and inserts (key, <output>) into map_.
  // The program name (argv[0]) should be an absolute path. The function
  // checks if the program exists before executing it as some programs
  // don't exist on Linux desktop.
  bool GetSingleValueFromTool(int argc, const char* argv[],
                              const std::string& key);

  // Parses name-value pairs from the file.
  void GetNameValuePairsFromFile(const FilePath& file_path,
                                 const std::string& eq,
                                 const std::string& delim);

  // These will parse strings with output in the format:
  // <key><EQ><value><DELIM>[<key><EQ><value>][...]
  // e.g. ParseNameValuePairs("key1=value1 key2=value2", "=", " ")
  bool ParseNameValuePairs(const std::string& in_string,
                           const std::string& eq,
                           const std::string& delim);

  // This version allows for values which end with a comment
  // beginning with comment_delim.
  // e.g."key2=value2 # Explanation of value\n"
  bool ParseNameValuePairsWithComments(const std::string& in_string,
                                       const std::string& eq,
                                       const std::string& delim,
                                       const std::string& comment_delim);

  bool ParseNameValuePairsFromTool(
      int argc,
      const char* argv[],
      const std::string& eq,
      const std::string& delim,
      const std::string& comment_delim);

 private:
  NameValueMap* map_;

  DISALLOW_COPY_AND_ASSIGN(NameValuePairsParser);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
