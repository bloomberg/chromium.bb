// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/profile_reset/jtl_parser.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/re2/re2/re2.h"

namespace {

// RegEx that matches the first line of a text. Will throw away any potential
// double-slash-introduced comments and the potential trailing EOL character.
// Note: will fail in case the first line contains an unmatched double-quote
// outside of comments.
const char kSingleLineWithMaybeCommentsRE[] =
    // Non-greedily match and capture sequences of 1.) string literals inside
    // correctly matched double-quotes, or 2.) any other character.
    "^((?:\"[^\"\\n]*\"|[^\"\\n])*?)"
    // Greedily match and throw away the potential comment.
    "(?://.*)?"
    // Match and throw away EOL, or match end-of-string.
    "(?:\n|$)";

// RegEx to match either a double-quote-enclosed string literal or a whitespace.
// Applied repeatedly and without overlapping, can be used to remove whitespace
// outside of string literals.
const char kRemoveWhitespaceRE[] = "(\"[^\"]*\")|\\s";

// The substitution pattern to use together with the above when replacing. As
// the whitespace is not back-referenced here, it will get removed.
const char kRemoveWhitespaceRewrite[] = "\\1";

// Separator to terminate a sentence.
const char kEndOfSentenceSeparator[] = ";";

// The 'true' Boolean keyword.
const char kTrueKeyword[] = "true";

// RegEx that matches and captures one argument, which is either a double-quote
// enclosed string, or a Boolean value. Will throw away a trailing comma.
const char kSingleArgumentRE[] = "(?:(?:\"([^\"]*)\"|(true|false))(?:,|$))";

// RegEx-es that, when concatenated, will match a single operation, and capture
// the: operation name, the optional arguments, and the separator that follows.
const char kOperationNameRE[] = "([[:word:]]+)";
const char kMaybeArgumentListRE[] =
    "(?:\\("                    // Opening parenthesis.
    "((?:\"[^\"]*\"|[^\")])*)"  // Capture: anything inside, quote-aware.
    "\\))?";                    // Closing parenthesis + everything optional.
const char kOperationSeparatorRE[] = "(;|\\.)";

}  // namespace

struct JtlParser::ParsingState {
  explicit ParsingState(const re2::StringPiece& compacted_source)
      : single_operation_regex(std::string(kOperationNameRE) +
                               kMaybeArgumentListRE +
                               kOperationSeparatorRE),
        single_argument_regex(kSingleArgumentRE),
        remaining_compacted_source(compacted_source),
        last_line_number(0) {}

  RE2 single_operation_regex;
  RE2 single_argument_regex;
  re2::StringPiece remaining_compacted_source;
  re2::StringPiece last_context;
  size_t last_line_number;
};

JtlParser::JtlParser(const std::string& compacted_source_code,
                     const std::vector<size_t>& newline_indices)
    : compacted_source_(compacted_source_code),
      newline_indices_(newline_indices) {
  state_.reset(new ParsingState(compacted_source_));
}

JtlParser::~JtlParser() {}

// static
bool JtlParser::RemoveCommentsAndAllWhitespace(
    const std::string& verbose_text,
    std::string* compacted_text,
    std::vector<size_t>* newline_indices,
    size_t* error_line_number) {
  DCHECK(compacted_text);
  DCHECK(newline_indices);
  std::string line;
  RE2 single_line_regex(kSingleLineWithMaybeCommentsRE);
  RE2 remove_whitespace_regex(kRemoveWhitespaceRE);
  re2::StringPiece verbose_text_piece(verbose_text);
  compacted_text->clear();
  newline_indices->clear();
  while (!verbose_text_piece.empty()) {
    if (!RE2::Consume(&verbose_text_piece, single_line_regex, &line)) {
      if (error_line_number)
        *error_line_number = newline_indices->size();
      return false;
    }
    RE2::GlobalReplace(
        &line, remove_whitespace_regex, kRemoveWhitespaceRewrite);
    *compacted_text += line;
    newline_indices->push_back(compacted_text->size());
  }
  return true;
}

bool JtlParser::HasFinished() {
  return state_->remaining_compacted_source.empty();
}

bool JtlParser::ParseNextOperation(std::string* name,
                                   base::ListValue* argument_list,
                                   bool* ends_sentence) {
  DCHECK(name);
  DCHECK(argument_list);
  DCHECK(ends_sentence);

  state_->last_context = state_->remaining_compacted_source;
  state_->last_line_number = GetOriginalLineNumber(
      compacted_source_.size() - state_->remaining_compacted_source.length());

  std::string arguments, separator;
  if (!RE2::Consume(&state_->remaining_compacted_source,
                    state_->single_operation_regex,
                    name,
                    &arguments,
                    &separator))
    return false;

  *ends_sentence = (separator == kEndOfSentenceSeparator);
  state_->last_context.remove_suffix(state_->remaining_compacted_source.size());

  re2::StringPiece arguments_piece(arguments);
  std::string string_value, boolean_value;
  while (!arguments_piece.empty()) {
    if (!RE2::Consume(&arguments_piece,
                      state_->single_argument_regex,
                      &string_value,
                      &boolean_value))
      return false;

    if (!boolean_value.empty()) {
      argument_list->Append(
          new base::FundamentalValue(boolean_value == kTrueKeyword));
    } else {
      // |string_value| might be empty for an empty string
      argument_list->Append(new base::StringValue(string_value));
    }
  }
  return true;
}

size_t JtlParser::GetOriginalLineNumber(size_t compacted_index) const {
  return static_cast<size_t>(std::upper_bound(newline_indices_.begin(),
                                              newline_indices_.end(),
                                              compacted_index) -
                             newline_indices_.begin());
}

size_t JtlParser::GetLastLineNumber() const { return state_->last_line_number; }

std::string JtlParser::GetLastContext() const {
  return state_->last_context.ToString();
}
