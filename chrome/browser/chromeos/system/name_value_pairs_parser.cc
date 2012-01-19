// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/name_value_pairs_parser.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace chromeos {  // NOLINT
namespace system {

namespace {

const char kQuoteChars[] = "\"";
const char kTrimChars[] = "\" ";

bool GetToolOutput(int argc, const char* argv[], std::string& output) {
  DCHECK_GE(argc, 1);

  if (!file_util::PathExists(FilePath(argv[0]))) {
    LOG(WARNING) << "Tool for statistics not found: " << argv[0];
    return false;
  }

  CommandLine command_line(argc, argv);
  if (!base::GetAppOutput(command_line, &output)) {
    LOG(WARNING) << "Error executing: " << command_line.GetCommandLineString();
    return false;
  }

  return true;
}

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
  return ParseNameValuePairsWithComments(in_string, eq, delim, "");
}

bool NameValuePairsParser::ParseNameValuePairsWithComments(
    const std::string& in_string,
    const std::string& eq,
    const std::string& delim,
    const std::string& comment_delim) {
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
    std::string key;
    std::string value;
    if (keyvalue.GetNext()) {
      TrimString(keyvalue.token(), kTrimChars, &key);
      if (keyvalue.GetNext()) {
        value = keyvalue.token();
        if (keyvalue.GetNext()) {
          LOG(WARNING) << "Multiple key tokens: '" << pair << "'. Aborting.";
          return false;
        }
        // If value ends with a comment, throw away everything after
        // comment_delim is encountered.
        if (!comment_delim.empty()) {
          StringTokenizer value_with_comment(value, comment_delim);
          value_with_comment.GetNext();
          value = value_with_comment.token();
        }

        TrimString(value, kTrimChars, &value);
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
  std::string output_string;
  if (!GetToolOutput(argc, argv, output_string))
    return false;

  TrimWhitespaceASCII(output_string, TRIM_ALL, &output_string);
  AddNameValuePair(key, output_string);
  return true;
}

void NameValuePairsParser::GetNameValuePairsFromFile(const FilePath& file_path,
                                                     const std::string& eq,
                                                     const std::string& delim) {
  std::string contents;
  if (file_util::ReadFileToString(file_path, &contents)) {
    ParseNameValuePairs(contents, eq, delim);
  } else {
    LOG(WARNING) << "Unable to read statistics file: " << file_path.value();
  }
}

bool NameValuePairsParser::ParseNameValuePairsFromTool(
    int argc,
    const char* argv[],
    const std::string& eq,
    const std::string& delim,
    const std::string& comment_delim) {
  std::string output_string;
  if (!GetToolOutput(argc, argv, output_string))
    return false;

  return ParseNameValuePairsWithComments(
      output_string, eq, delim, comment_delim);
}

}  // namespace system
}  // namespace chromeos
