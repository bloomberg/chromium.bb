// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/tokenized_string_match.h"

#include "chrome/browser/ui/app_list/search/tokenized_string_char_iterator.h"

namespace app_list {

namespace {

// The factors below are applied when the current char of query matches
// the current char of the text to be matched. Different factors are chosen
// based on where the match happens. kIsPrefixMultiplier is used when the
// matched portion is a prefix of both the query and the text, which implies
// that the matched chars are at the same position in query and text. This is
// the most preferred case thus it has the highest score. When the current char
// of the query and the text does not match, the algorithm moves to the next
// token in the text and try to match from there. kIsFrontOfWordMultipler will
// be used if the first char of the token matches the current char of the query.
// Otherwise, the match is considered as weak and kIsWeakHitMultiplier is
// used.
// Examples:
//   Suppose the text to be matched is 'Google Chrome'.
//   Query 'go' would yield kIsPrefixMultiplier for each char.
//   Query 'gc' would use kIsPrefixMultiplier for 'g' and
//       kIsFrontOfWordMultipler for 'c'.
//   Query 'ch' would use kIsFrontOfWordMultipler for 'c' and
//       kIsWeakHitMultiplier for 'h'.
const double kIsPrefixMultiplier = 1.0;
const double kIsFrontOfWordMultipler = 0.8;
const double kIsWeakHitMultiplier = 0.6;

// A relevance score that represents no match.
const double kNoMatchScore = 0.0;

}  // namespace

TokenizedStringMatch::TokenizedStringMatch()
    : relevance_(kNoMatchScore) {}

TokenizedStringMatch::~TokenizedStringMatch() {}

bool TokenizedStringMatch::Calculate(const TokenizedString& query,
                                     const TokenizedString& text) {
  relevance_ = kNoMatchScore;
  hits_.clear();

  gfx::Range hit = gfx::Range::InvalidRange();

  TokenizedStringCharIterator query_iter(query);
  TokenizedStringCharIterator text_iter(text);

  while (!query_iter.end() && !text_iter.end()) {
    if (query_iter.Get() == text_iter.Get()) {
      if (query_iter.GetArrayPos() == text_iter.GetArrayPos())
        relevance_ += kIsPrefixMultiplier;
      else if (text_iter.IsFirstCharOfToken())
        relevance_ += kIsFrontOfWordMultipler;
      else
        relevance_ += kIsWeakHitMultiplier;

      if (!hit.IsValid())
        hit.set_start(text_iter.GetArrayPos());
      hit.set_end(text_iter.GetArrayPos() + text_iter.GetCharSize());

      query_iter.NextChar();
      text_iter.NextChar();
    } else {
      if (hit.IsValid()) {
        hits_.push_back(hit);
        hit = gfx::Range::InvalidRange();
      }

      text_iter.NextToken();
    }
  }

  // No match if query is not fully consumed.
  if (!query_iter.end()) {
    relevance_ = kNoMatchScore;
    hits_.clear();
    return false;
  }

  if (hit.IsValid())
    hits_.push_back(hit);

  // Using length() for normalizing is not 100% correct but should be good
  // enough compared with using real char count of the text.
  if (text.text().length())
    relevance_ /= text.text().length();

  return relevance_ > kNoMatchScore;
}

bool TokenizedStringMatch::Calculate(const string16& query,
                                     const string16& text) {
  const TokenizedString tokenized_query(query);
  const TokenizedString tokenized_text(text);
  return Calculate(tokenized_query, tokenized_text);
}

}  // namespace app_list
