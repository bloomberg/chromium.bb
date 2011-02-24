// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/history_provider_util.h"
#include "chrome/browser/history/history_types.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace base {
class Time;
}

namespace history {

class URLDatabase;

// Used for intermediate history result operations.
struct ScoredHistoryMatch : public HistoryMatch {
  // Required for STL, we don't use this directly.
  ScoredHistoryMatch();

  ScoredHistoryMatch(const URLRow& url_info,
                     size_t input_location,
                     bool match_in_scheme,
                     bool innermost_match,
                     int score);

  // An interim score taking into consideration location and completeness
  // of the match.
  int raw_score;
};
typedef std::vector<ScoredHistoryMatch> ScoredHistoryMatches;

// The URL history source.
// Holds portions of the URL database in memory in an indexed form.  Used to
// quickly look up matching URLs for a given query string.  Used by
// the HistoryURLProvider for inline autocomplete and to provide URL
// matches to the omnibox.
//
// Note about multi-byte codepoints and the data structures in the
// InMemoryURLIndex class: One will quickly notice that no effort is made to
// insure that multi-byte character boundaries are detected when indexing the
// words and characters in the URL history database except when converting
// URL strings to lowercase. Multi-byte-edness makes no difference when
// indexing or when searching the index as the final filtering of results
// is dependent on the comparison of a string of bytes, not individual
// characters. While the lookup of those bytes during a search in the
// |char_word_map_| could serve up words in which the individual char16
// occurs as a portion of a composite character the next filtering step
// will eliminate such words except in the case where a single character
// is being searched on and which character occurs as the second char16 of a
// multi-char16 instance.
class InMemoryURLIndex {
 public:
  InMemoryURLIndex();
  ~InMemoryURLIndex();

  // Convenience types
  typedef std::vector<string16> String16Vector;

  // Open and index the URL history database.
  // |languages| gives a list of language encodings with which the history
  // URLs and omnibox searches are interpreted, i.e. when each is broken
  // down into words and each word is broken down into characters.
  bool Init(URLDatabase* history_db, const std::string& languages);

  // Reset the history index.
  void Reset();

  // Given a vector containing one or more words as string16s, scan the
  // history index and return a vector with all scored, matching history items.
  // Each term must occur somewhere in the history item for the item to
  // qualify; however, the terms do not necessarily have to be adjacent.
  // Results are sorted with higher scoring items first.
  ScoredHistoryMatches HistoryItemsForTerms(const String16Vector& terms);

  // Returns the date threshold for considering an history item as significant.
  static base::Time RecentThreshold();

 private:
  friend class AddHistoryMatch;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST(InMemoryURLIndexTest, Initialization);

  // Convenience types.
  typedef std::set<string16> String16Set;
  typedef std::set<char16> Char16Set;

  // An index into list of all of the words we have indexed.
  typedef int16 WordID;

  // A map allowing a WordID to be determined given a word.
  typedef std::map<string16, WordID> WordMap;

  // A map from character to word_ids.
  typedef std::set<WordID> WordIDSet;  // An index into the WordList.
  typedef std::map<char16, WordIDSet> CharWordIDMap;

  // A map from word_id to history item.
  // TODO(mrossetti): URLID is 64 bit: a memory bloat and performance hit.
  // Consider using a smaller type.
  typedef URLID HistoryID;
  typedef std::set<HistoryID> HistoryIDSet;
  typedef std::map<WordID, HistoryIDSet> WordIDHistoryMap;

  // Support caching of term character intersections so that we can optimize
  // searches which build upon a previous search.
  struct TermCharWordSet;
  typedef std::vector<TermCharWordSet> TermCharWordSetVector;

  // TODO(rohitrao): Probably replace this with QueryResults.
  typedef std::vector<URLRow> URLRowVector;

  // A map from history_id to the history's URL and title.
  typedef std::map<HistoryID, URLRow> HistoryInfoMap;

  // A helper class which performs the final filter on each candidate
  // history URL match, inserting accepted matches into |scored_matches_|
  // and trimming the maximum number of matches to 10.
  class AddHistoryMatch : public std::unary_function<HistoryID, void> {
   public:
    AddHistoryMatch(const InMemoryURLIndex& index,
                    const String16Vector& lower_terms);
    ~AddHistoryMatch();

    void operator()(const HistoryID history_id);

    ScoredHistoryMatches ScoredMatches() const { return scored_matches_; }

   private:
    const InMemoryURLIndex& index_;
    ScoredHistoryMatches scored_matches_;
    const String16Vector& lower_terms_;
  };

  // Break a string down into individual words.
  static String16Set WordSetFromString16(const string16& uni_string);

  // Given a set of Char16s, find words containing those characters. If any
  // existing, cached set is a proper subset then start with that cached
  // set. Update the cache.
  WordIDSet WordIDSetForTermChars(const Char16Set& uni_chars);

  // URL History indexing support functions.

  // Index one URL history item.
  bool IndexRow(const URLRow& row);

  // Break a string down into its individual characters.
  // Note that this is temporarily intended to work on a single word, but
  // _will_ work on a string of words, perhaps with unexpected results.
  // TODO(mrossetti): Lots of optimizations possible here for not restarting
  // a search if the user is just typing along. Also, change this to uniString
  // and properly handle substring matches, scoring and sorting the results
  // by score. Also, provide the metrics for where the matches occur so that
  // the UI can highlight the matched sections.
  Char16Set CharactersFromString16(const string16& uni_word);

  // Given a single word, add a reference to the containing history item
  // to the index.
  void AddWordToIndex(const string16& uni_word, HistoryID history_id);

  // Update an existing entry in the word/history index by adding the
  // |history_id| to set for |word_id| in the word_id_history_map_.
  void UpdateWordHistory(WordID word_id, HistoryID history_id);

  // Create a new entry in the word/history map for |word_id| and add
  // |history_id| as the initial element of the word's set.
  void AddWordHistory(const string16& uni_word, HistoryID history_id);

  // Clear the search term cache. This cache holds on to the intermediate
  // word results for each previously typed character to eliminate the need
  // to re-perform set operations for previously typed characters.
  void ResetTermCharWordSetCache();

  // Compose a set of history item IDs by intersecting the set for each word
  // in |uni_string|.
  HistoryIDSet HistoryIDSetFromWords(const string16& uni_string);

  // Helper function to HistoryIDSetFromWords which composes a set of history
  // ids for the given term given in |uni_word|.
  HistoryIDSet HistoryIDsForTerm(const string16& uni_word);

  // Calculate a raw score for this history item by first determining
  // if all of the terms in |terms_vector| occur in |row| and, if so,
  // calculating a raw score based on 1) starting position of each term
  // in the user input, 2) completeness of each term's match, 3) ordering
  // of the occurrence of each term (i.e. they appear in order), 4) last
  // visit time, and 5) number of visits.
  // This raw score allows the results to be ordered and can be used
  // to influence the final score calculated by the client of this
  // index. Return the starting location of the first term in
  // |first_term_location|.
  static int RawScoreForURL(const URLRow& row,
                            const String16Vector& terms_vector,
                            size_t* first_term_location);

  // A list of all of indexed words. The index of a word in this list is the
  // ID of the word in the word_map_. It reduces the memory overhead by
  // replacing a potentially long and repeated string with a simple index.
  // NOTE: A word will _never_ be removed from this vector thus insuring
  // the immutability of the word_id throughout the session, reducing
  // maintenance complexity.
  String16Vector word_list_;

  uint64 history_item_count_;
  WordMap word_map_;
  CharWordIDMap char_word_map_;
  WordIDHistoryMap word_id_history_map_;
  TermCharWordSetVector term_char_word_set_cache_;
  HistoryInfoMap history_info_map_;
  std::string languages_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLIndex);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
