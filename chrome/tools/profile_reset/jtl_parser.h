// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_PROFILE_RESET_JTL_PARSER_H_
#define CHROME_TOOLS_PROFILE_RESET_JTL_PARSER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

// Parses text-based JTL source code into a stream of operation names, arguments
// and separator kinds.
class JtlParser {
 public:
  // Creates a new parser to parse |compacted_source_code|, which should already
  // be stripped of all comments and whitespace (except inside string literals).
  // Use RemoveCommentsAndAllWhitespace() to manufacture these arguments, also
  // see its comments for a description of |newline_indices|.
  JtlParser(const std::string& compacted_source_code,
            const std::vector<size_t>& newline_indices);
  ~JtlParser();

  // Removes comments from |verbose_text| and compacts it into whitespace-free
  // format (except inside string literals). Elements in |newline_indices| will
  // be monotonically increasing and will refer to positions in |compacted_text|
  // such that a new line has been removed before that position.
  // Example:
  //   verbose_text = "H e l l o // my\n"
  //                  " dear \n"
  //                  "\n"
  //                  "world\" ! \""
  //   compacted_text = "Hellodearworld\" ! \""
  //                     01234567890123...
  //   newline_indices = {5, 9, 9}
  // Returns true on success, false if there were unmatched quotes in a line, in
  // which case |error_line_number| will be set accordingly if it is non-NULL.
  static bool RemoveCommentsAndAllWhitespace(
      const std::string& verbose_text,
      std::string* compacted_text,
      std::vector<size_t>* newline_indices,
      size_t* error_line_number);

  // Returns true if the entire input has been successfully consumed. Note that
  // even when this returns false, a subsequent call to ParseNextOperation()
  // might still fail if the next operation cannot be parsed.
  bool HasFinished();

  // Fetches the |name| and the |argument_list| of the next operation, and also
  // whether or not it |ends_the_sentence|, i.e. it is followed by the
  // end-of-sentence separator.
  // Returns false if there is a parsing error, in which case the values for the
  // output parameters are undefined, and |this| parser shall no longer be used.
  bool ParseNextOperation(std::string* name,
                          base::ListValue* argument_list,
                          bool* ends_the_sentence);

  // Returns the compacted source code that was passed in to the constructor.
  const std::string& compacted_source() const { return compacted_source_; }

  // Returns at which line the character at position |compacted_index| in the
  // |compacted_source()| was originally located.
  size_t GetOriginalLineNumber(size_t compacted_index) const;

  size_t GetLastLineNumber() const;
  std::string GetLastContext() const;

 private:
  // Contains pre-compiled regular expressions and related state. Factored out
  // to avoid this header depending on RE2 headers.
  struct ParsingState;

  std::string compacted_source_;
  std::vector<size_t> newline_indices_;
  scoped_ptr<ParsingState> state_;

  DISALLOW_COPY_AND_ASSIGN(JtlParser);
};

#endif  // CHROME_TOOLS_PROFILE_RESET_JTL_PARSER_H_
