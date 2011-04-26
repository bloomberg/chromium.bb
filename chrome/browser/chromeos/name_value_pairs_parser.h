// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NAME_VALUE_PAIRS_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_NAME_VALUE_PAIRS_PARSER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"

namespace chromeos {

// The parser is used to get machine info as name-value pairs. Defined
// here to be accessable by tests.
class NameValuePairsParser {
 public:
  typedef std::map<std::string, std::string> NameValueMap;

  // The obtained info will be written into machine_info.
  explicit NameValuePairsParser(NameValueMap* map);

  void AddNameValuePair(const std::string& key, const std::string& value);

  // Executes tool and inserts (key, <output>) into map_.
  bool GetSingleValueFromTool(int argc, const char* argv[],
                              const std::string& key);
  // Executes tool, parses the output using ParseNameValuePairs,
  // and inserts the results into name_value_pairs_.
  bool ParseNameValuePairsFromTool(int argc, const char* argv[],
                                   const std::string& eq,
                                   const std::string& delim);

 private:
  // This will parse strings with output in the format:
  // <key><EQ><value><DELIM>[<key><EQ><value>][...]
  // e.g. ParseNameValuePairs("key1=value1 key2=value2", "=", " ")
  bool ParseNameValuePairs(const std::string& in_string,
                           const std::string& eq,
                           const std::string& delim);

  NameValueMap* map_;

  DISALLOW_COPY_AND_ASSIGN(NameValuePairsParser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NAME_VALUE_PAIRS_PARSER_H_
