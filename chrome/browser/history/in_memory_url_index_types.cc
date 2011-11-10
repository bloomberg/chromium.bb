// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index_types.h"

#include <algorithm>
#include <iterator>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/string_util.h"

namespace history {

// Matches within URL and Title Strings ----------------------------------------

TermMatches MatchTermInString(const string16& term,
                              const string16& string,
                              int term_num) {
  const size_t kMaxCompareLength = 2048;
  const string16& short_string = (string.length() > kMaxCompareLength) ?
      string.substr(0, kMaxCompareLength) : string;
  TermMatches matches;
  for (size_t location = short_string.find(term); location != string16::npos;
       location = short_string.find(term, location + 1))
    matches.push_back(TermMatch(term_num, location, term.length()));
  return matches;
}

// Comparison function for sorting TermMatches by their offsets.
bool MatchOffsetLess(const TermMatch& m1, const TermMatch& m2) {
  return m1.offset < m2.offset;
}

TermMatches SortAndDeoverlapMatches(const TermMatches& matches) {
  if (matches.empty())
    return matches;
  TermMatches sorted_matches = matches;
  std::sort(sorted_matches.begin(), sorted_matches.end(), MatchOffsetLess);
  TermMatches clean_matches;
  TermMatch last_match;
  for (TermMatches::const_iterator iter = sorted_matches.begin();
       iter != sorted_matches.end(); ++iter) {
    if (iter->offset >= last_match.offset + last_match.length) {
      last_match = *iter;
      clean_matches.push_back(last_match);
    }
  }
  return clean_matches;
}

std::vector<size_t> OffsetsFromTermMatches(const TermMatches& matches) {
  std::vector<size_t> offsets;
  for (TermMatches::const_iterator i = matches.begin(); i != matches.end(); ++i)
    offsets.push_back(i->offset);
  return offsets;
}

TermMatches ReplaceOffsetsInTermMatches(const TermMatches& matches,
                                        const std::vector<size_t>& offsets) {
  DCHECK_EQ(matches.size(), offsets.size());
  TermMatches new_matches;
  std::vector<size_t>::const_iterator offset_iter = offsets.begin();
  for (TermMatches::const_iterator term_iter = matches.begin();
       term_iter != matches.end(); ++term_iter, ++offset_iter) {
    if (*offset_iter != string16::npos) {
      TermMatch new_match(*term_iter);
      new_match.offset = *offset_iter;
      new_matches.push_back(new_match);
    }
  }
  return new_matches;
}

// ScoredHistoryMatch ----------------------------------------------------------

ScoredHistoryMatch::ScoredHistoryMatch()
    : raw_score(0),
      can_inline(false) {}

ScoredHistoryMatch::ScoredHistoryMatch(const URLRow& url_info)
    : HistoryMatch(url_info, 0, false, false),
      raw_score(0),
      can_inline(false) {}

ScoredHistoryMatch::~ScoredHistoryMatch() {}

// Comparison function for sorting ScoredMatches by their scores.
bool ScoredHistoryMatch::MatchScoreGreater(const ScoredHistoryMatch& m1,
                                           const ScoredHistoryMatch& m2) {
  return m1.raw_score > m2.raw_score;
}

// InMemoryURLIndex's Private Data ---------------------------------------------

URLIndexPrivateData::URLIndexPrivateData() {}

URLIndexPrivateData::~URLIndexPrivateData() {}

void URLIndexPrivateData::Clear() {
  word_list_.clear();
  available_words_.clear();
  word_map_.clear();
  char_word_map_.clear();
  word_id_history_map_.clear();
  history_id_word_map_.clear();
  history_info_map_.clear();
}

void URLIndexPrivateData::AddToHistoryIDWordMap(HistoryID history_id,
                                               WordID word_id) {
  HistoryIDWordMap::iterator iter = history_id_word_map_.find(history_id);
  if (iter != history_id_word_map_.end()) {
    WordIDSet& word_id_set(iter->second);
    word_id_set.insert(word_id);
  } else {
    WordIDSet word_id_set;
    word_id_set.insert(word_id);
    history_id_word_map_[history_id] = word_id_set;
  }
}

WordIDSet URLIndexPrivateData::WordIDSetForTermChars(
    const Char16Set& term_chars) {
  WordIDSet word_id_set;
  for (Char16Set::const_iterator c_iter = term_chars.begin();
       c_iter != term_chars.end(); ++c_iter) {
    CharWordIDMap::iterator char_iter = char_word_map_.find(*c_iter);
    if (char_iter == char_word_map_.end()) {
      // A character was not found so there are no matching results: bail.
      word_id_set.clear();
      break;
    }
    WordIDSet& char_word_id_set(char_iter->second);
    // It is possible for there to no longer be any words associated with
    // a particular character. Give up in that case.
    if (char_word_id_set.empty()) {
      word_id_set.clear();
      break;
    }

    if (c_iter == term_chars.begin()) {
      // First character results becomes base set of results.
      word_id_set = char_word_id_set;
    } else {
      // Subsequent character results get intersected in.
      WordIDSet new_word_id_set;
      std::set_intersection(word_id_set.begin(), word_id_set.end(),
                            char_word_id_set.begin(), char_word_id_set.end(),
                            std::inserter(new_word_id_set,
                                          new_word_id_set.begin()));
      word_id_set.swap(new_word_id_set);
    }
  }
  return word_id_set;
}

// Utility Functions -----------------------------------------------------------

String16Set String16SetFromString16(const string16& uni_string) {
  const size_t kMaxWordLength = 64;
  String16Vector words = String16VectorFromString16(uni_string, false);
  String16Set word_set;
  for (String16Vector::const_iterator iter = words.begin(); iter != words.end();
       ++iter)
    word_set.insert(base::i18n::ToLower(*iter).substr(0, kMaxWordLength));
  return word_set;
}

String16Vector String16VectorFromString16(const string16& uni_string,
                                          bool break_on_space) {
  base::i18n::BreakIterator iter(uni_string,
      break_on_space ? base::i18n::BreakIterator::BREAK_SPACE :
          base::i18n::BreakIterator::BREAK_WORD);
  String16Vector words;
  if (!iter.Init())
    return words;
  while (iter.Advance()) {
    if (break_on_space || iter.IsWord()) {
      string16 word = iter.GetString();
      if (break_on_space)
        TrimWhitespace(word, TRIM_ALL, &word);
      if (!word.empty())
        words.push_back(word);
    }
  }
  return words;
}

Char16Set Char16SetFromString16(const string16& term) {
  Char16Set characters;
  for (string16::const_iterator iter = term.begin(); iter != term.end(); ++iter)
    characters.insert(*iter);
  return characters;
}

}  // namespace history
