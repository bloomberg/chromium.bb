// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_TYPES_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_TYPES_H_

#include <map>
#include <set>
#include <vector>

#include "base/strings/string16.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history {

// The maximum number of characters to consider from an URL and page title
// while matching user-typed terms.
const size_t kMaxSignificantChars = 200;

// Matches within URL and Title Strings ----------------------------------------

// Specifies where an omnibox term occurs within a string. Used for specifying
// highlights in AutocompleteMatches (ACMatchClassifications) and to assist in
// scoring a result.
struct TermMatch {
  TermMatch() : term_num(0), offset(0), length(0) {}
  TermMatch(int term_num, size_t offset, size_t length)
      : term_num(term_num),
        offset(offset),
        length(length) {}

  int term_num;  // The index of the term in the original search string.
  size_t offset;  // The starting offset of the substring match.
  size_t length;  // The length of the substring match.
};
typedef std::vector<TermMatch> TermMatches;

// Returns a TermMatches which has an entry for each occurrence of the
// string |term| found in the string |cleaned_string|. Use
// CleanUpUrlForMatching() or CleanUpUrlTitleMatching() before passing
// |cleaned_string| to this function. The function marks each match
// with |term_num| so that the resulting TermMatches can be merged
// with other TermMatches for other terms. Note that only the first
// 2,048 characters of |string| are considered during the match
// operation.
TermMatches MatchTermInString(const base::string16& term,
                              const base::string16& cleaned_string,
                              int term_num);

// Sorts and removes overlapping substring matches from |matches| and
// returns the cleaned up matches.
TermMatches SortAndDeoverlapMatches(const TermMatches& matches);

// Extracts and returns the offsets from |matches|.  This includes both
// the offsets corresponding to the beginning of a match and the offsets
// corresponding to the end of a match (i.e., offset+length for that match).
std::vector<size_t> OffsetsFromTermMatches(const TermMatches& matches);

// Replaces the offsets and lengths in |matches| with those given in |offsets|.
// |offsets| gives beginning and ending offsets for each match; this function
// translates (beginning, ending) offset into (beginning offset, length of
// match).  It deletes any matches for which an endpoint is npos and returns
// the updated list of matches.
TermMatches ReplaceOffsetsInTermMatches(const TermMatches& matches,
                                        const std::vector<size_t>& offsets);

// Convenience Types -----------------------------------------------------------

typedef std::vector<base::string16> String16Vector;
typedef std::set<base::string16> String16Set;
typedef std::set<base::char16> Char16Set;
typedef std::vector<base::char16> Char16Vector;

// A vector that contains the offsets at which each word starts within a string.
typedef std::vector<size_t> WordStarts;

// Utility Functions -----------------------------------------------------------

// Breaks the string |cleaned_uni_string| down into individual words.
// Use CleanUpUrlForMatching() or CleanUpUrlTitleMatching() before
// passing |cleaned_uni_string| to this function. If |word_starts| is
// not NULL then clears and pushes the offsets within
// |cleaned_uni_string| at which each word starts onto
// |word_starts|. These offsets are collected only up to the first
// kMaxSignificantChars of |cleaned_uni_string|.
String16Set String16SetFromString16(const base::string16& cleaned_uni_string,
                                    WordStarts* word_starts);

// Breaks the |cleaned_uni_string| string down into individual words
// and return a vector with the individual words in their original
// order.  Use CleanUpUrlForMatching() or CleanUpUrlTitleMatching()
// before passing |cleaned_uni_string| to this function.  If
// |break_on_space| is false then the resulting list will contain only
// words containing alpha-numeric characters. If |break_on_space| is
// true then the resulting list will contain strings broken at
// whitespace. (|break_on_space| indicates that the
// BreakIterator::BREAK_SPACE (equivalent to BREAK_LINE) approach is
// to be used. For a complete description of this algorithm refer to
// the comments in base/i18n/break_iterator.h.) If |word_starts| is
// not NULL then clears and pushes the word starts onto |word_starts|.
//
// Example:
//   Given: |cleaned_uni_string|: "http://www.google.com/ harry the rabbit."
//   With |break_on_space| false the returned list will contain:
//    "http", "www", "google", "com", "harry", "the", "rabbit"
//   With |break_on_space| true the returned list will contain:
//    "http://", "www.google.com/", "harry", "the", "rabbit."
String16Vector String16VectorFromString16(
    const base::string16& cleaned_uni_string,
    bool break_on_space,
    WordStarts* word_starts);

// Breaks the |uni_word| string down into its individual characters.
// Note that this is temporarily intended to work on a single word, but
// _will_ work on a string of words, perhaps with unexpected results.
// TODO(mrossetti): Lots of optimizations possible here for not restarting
// a search if the user is just typing along. Also, change this to uniString
// and properly handle substring matches, scoring and sorting the results
// by score. Also, provide the metrics for where the matches occur so that
// the UI can highlight the matched sections.
Char16Set Char16SetFromString16(const base::string16& uni_word);

// Support for InMemoryURLIndex Private Data -----------------------------------

// An index into a list of all of the words we have indexed.
typedef size_t WordID;

// A map allowing a WordID to be determined given a word.
typedef std::map<base::string16, WordID> WordMap;

// A map from character to the word_ids of words containing that character.
typedef std::set<WordID> WordIDSet;  // An index into the WordList.
typedef std::map<base::char16, WordIDSet> CharWordIDMap;

// A map from word (by word_id) to history items containing that word.
typedef history::URLID HistoryID;
typedef std::set<HistoryID> HistoryIDSet;
typedef std::vector<HistoryID> HistoryIDVector;
typedef std::map<WordID, HistoryIDSet> WordIDHistoryMap;
typedef std::map<HistoryID, WordIDSet> HistoryIDWordMap;


// Information used in scoring a particular URL.
typedef std::vector<VisitInfo> VisitInfoVector;
struct HistoryInfoMapValue {
  HistoryInfoMapValue();
  ~HistoryInfoMapValue();

  // This field is always populated.
  URLRow url_row;

  // This field gets filled in asynchronously after a visit.  As such,
  // it's almost always correct.  If it's wrong, it's likely to either
  // be empty (if this URL was recently added to the index) or
  // slightly out-of-date (one visit behind).
  VisitInfoVector visits;
};

// A map from history_id to the history's URL and title.
typedef std::map<HistoryID, HistoryInfoMapValue> HistoryInfoMap;

// A map from history_id to URL and page title word start metrics.
struct RowWordStarts {
  RowWordStarts();
  ~RowWordStarts();

  // Clears both url_word_starts_ and title_word_starts_.
  void Clear();

  WordStarts url_word_starts_;
  WordStarts title_word_starts_;
};
typedef std::map<HistoryID, RowWordStarts> WordStartsMap;

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_TYPES_H_
