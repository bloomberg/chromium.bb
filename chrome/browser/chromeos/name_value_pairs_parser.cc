// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/name_value_pairs_parser.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace chromeos {  // NOLINT

namespace {

const char kQuoteChars[] = "\"";

}  // namespace

NameValuePairsParser::NameValuePairsParser(NameValueMap* map)
    : map_(map) {
}

void NameValuePairsParser::AddNameValuePair(const std::string& key,
                                            const std::string& value) {
  (*map_)[key] = value;
  VLOG(1) << "name: " << key << ", value: " << value;
}

bool NameValuePairsParser::ParseNameValuePairs(const std::string& in_string,
                                               const std::string& eq,
                                               const std::string& delim) {
  // Set up the pair tokenizer.
  StringTokenizer pair_toks(in_string, delim);
  pair_toks.set_quote_chars(kQuoteChars);
  // Process token pairs.
  while (pair_toks.GetNext()) {
    std::string pair(pair_toks.token());
    if (pair.find(eq) == 0) {
      LOG(WARNING) << "Empty key: '" << pair << "'. Aborting.";
      return false;
    }
    StringTokenizer keyvalue(pair, eq);
    std::string key,value;
    if (keyvalue.GetNext()) {
      TrimString(keyvalue.token(), kQuoteChars, &key);
      if (keyvalue.GetNext()) {
        TrimString(keyvalue.token(), kQuoteChars, &value);
        if (keyvalue.GetNext()) {
          LOG(WARNING) << "Multiple key tokens: '" << pair << "'. Aborting.";
          return false;
        }
      }
    }
    if (key.empty()) {
      LOG(WARNING) << "Invalid token pair: '" << pair << "'. Aborting.";
      return false;
    }
    AddNameValuePair(key, value);
  }
  return true;
}

bool NameValuePairsParser::GetSingleValueFromTool(int argc,
                                                  const char* argv[],
                                                  const std::string& key) {
  CommandLine command_line(argc, argv);
  std::string output_string;
  // TODO(stevenjb,satorux): Make this non blocking: crosbug.com/5603.
  base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
  if (argc < 1 || !base::GetAppOutput(command_line, &output_string)) {
    LOG(WARNING) << "Error excuting: " << command_line.command_line_string();
    return false;
  }
  TrimWhitespaceASCII(output_string, TRIM_ALL, &output_string);
  AddNameValuePair(key, output_string);
  return true;
}

bool NameValuePairsParser::ParseNameValuePairsFromTool(
    int argc,
    const char* argv[],
    const std::string& eq,
    const std::string& delim) {
  CommandLine command_line(argc, argv);
  std::string output_string;
  // TODO(stevenjb,satorux): Make this non blocking: crosbug.com/5603.
  base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
  if (argc < 1 || !base::GetAppOutput(command_line, &output_string)) {
    LOG(WARNING) << "Error excuting: " << command_line.command_line_string();
    return false;
  }
  if (!ParseNameValuePairs(output_string, eq, delim)) {
    LOG(WARNING) << "Error parsing values while excuting: "
                 << command_line.command_line_string();
    return false;
  }
  return true;
}

}  // namespace chromeos
