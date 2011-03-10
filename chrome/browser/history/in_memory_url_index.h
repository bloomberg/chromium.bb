// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_provider_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index_cache.pb.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class Profile;

namespace base {
class Time;
}

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {

namespace imui = in_memory_url_index;

class URLDatabase;

// Specifies where an omnibox term occurs within a string. Used for specifying
// highlights in AutocompleteMatches (ACMatchClassifications) and to assist in
// scoring a result.
struct TermMatch {
  TermMatch(int term_num, size_t offset, size_t length)
      : term_num(term_num),
        offset(offset),
        length(length) {}

  int term_num;  // The index of the term in the original search string.
  size_t offset;  // The starting offset of the substring match.
  size_t length;  // The length of the substring match.
};
typedef std::vector<TermMatch> TermMatches;

// Used for intermediate history result operations.
struct ScoredHistoryMatch : public HistoryMatch {
  ScoredHistoryMatch();  // Required by STL.
  explicit ScoredHistoryMatch(const URLRow& url_info);
  ~ScoredHistoryMatch();

  // An interim score taking into consideration location and completeness
  // of the match.
  int raw_score;
  TermMatches url_matches;  // Term matches within the URL.
  TermMatches title_matches;  // Term matches within the page title.
  size_t prefix_adjust;  // The length of a prefix which should be ignored.
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
  // |history_dir| is a path to the directory containing the history database
  // within the profile wherein the cache and transaction journals will be
  // stored.
  explicit InMemoryURLIndex(const FilePath& history_dir);
  ~InMemoryURLIndex();

  // Convenience types
  typedef std::vector<string16> String16Vector;

  // Opens and indexes the URL history database.
  // |languages| gives a list of language encodings with which the history
  // URLs and omnibox searches are interpreted, i.e. when each is broken
  // down into words and each word is broken down into characters.
  bool Init(URLDatabase* history_db, const std::string& languages);

  // Reloads the history index. Attempts to reload from the cache unless
  // |clear_cache| is true. If the cache is unavailable then reload the
  // index from |history_db|.
  bool ReloadFromHistory(URLDatabase* history_db, bool clear_cache);

  // Signals that any outstanding initialization should be canceled and
  // flushes the cache to disk.
  void ShutDown();

  // Restores the index's private data from the cache file stored in the
  // profile directory and returns true if successful.
  bool RestoreFromCacheFile();

  // Caches the index private data and writes the cache file to the profile
  // directory.
  bool SaveToCacheFile();

  // Given a vector containing one or more words as string16s, scans the
  // history index and return a vector with all scored, matching history items.
  // Each term must occur somewhere in the history item for the item to
  // qualify; however, the terms do not necessarily have to be adjacent.
  // Results are sorted with higher scoring items first. Each term from |terms|
  // may contain punctuation but should not contain spaces.
  ScoredHistoryMatches HistoryItemsForTerms(const String16Vector& terms);

  // Updates or adds an history item to the index if it meets the minimum
  // 'quick' criteria.
  void UpdateURL(URLID row_id, const URLRow& row);

  // Deletes indexing data for an history item. The item may not have actually
  // been indexed (which is the case if it did not previously meet minimum
  // 'quick' criteria).
  void DeleteURL(URLID row_id);

  // Breaks the |uni_string| string down into individual words and return
  // a vector with the individual words in their original order. Break on
  // whitespace if |break_on_space| also on special characters.
  static String16Vector WordVectorFromString16(const string16& uni_string,
                                               bool break_on_space);

 private:
  friend class AddHistoryMatch;
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheFilePath);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, Char16Utilities);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, Scoring);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, StaticFunctions);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleSearch);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TypedCharacterCaching);

  // Signals that there are no previously cached results for the typed term.
  static const size_t kNoCachedResultForTerm;

  // Creating one of me without a history path is not allowed (tests excepted).
  InMemoryURLIndex();

  // Convenience types.
  typedef std::set<string16> String16Set;
  typedef std::set<char16> Char16Set;
  typedef std::vector<char16> Char16Vector;

  // An index into list of all of the words we have indexed.
  typedef int WordID;

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

  // Support caching of term character results so that we can optimize
  // searches which build upon a previous search. Each entry in this vector
  // represents a progressive reduction of the result set for each unique
  // character found in the search term, with each character being taken as
  // initially encountered. For example, once the results for the search
  // term of "mextexarkana" have been fully determined, this vector will
  // contain one entry for the characters: m, e, x, t, a, r, k, & n, in
  // that order. The result sets will naturally shrink in size for each
  // subsequent character as the sets intersections are taken in an
  // incremental manner.
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

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void ClearPrivateData();

  // Breaks a string down into individual words.
  static String16Set WordSetFromString16(const string16& uni_string);

  // Given a vector of Char16s, representing the characters the user has typed
  // into the omnibox, finds words containing those characters. If any
  // existing, cached set is a proper subset then starts with that cached
  // set. Updates the previously-typed-character cache.
  WordIDSet WordIDSetForTermChars(const Char16Vector& uni_chars);

  // Given a vector of Char16s in |uni_chars|, compare those characters, in
  // order, with the previously searched term, returning the index of the
  // cached results in the term_char_word_set_cache_ of the set with best
  // matching number of characters. Returns kNoCachedResultForTerm if there
  // was no match at all (i.e. the first character of |uni-chars| is not the
  // same as the character of the first entry in term_char_word_set_cache_).
  size_t CachedResultsIndexForTerm(const Char16Vector& uni_chars);

  // Creates a TermMatches which has an entry for each occurrence of the string
  // |term| found in the string |string|. Mark each match with |term_num| so
  // that the resulting TermMatches can be merged with other TermMatches for
  // other terms.
  static TermMatches MatchTermInString(const string16& term,
                                       const string16& string,
                                       int term_num);

  // URL History indexing support functions.

  // Indexes one URL history item.
  bool IndexRow(const URLRow& row);

  // Breaks a string down into unique, individual characters in the order
  // in which the characters are first encountered in the |uni_word| string.
  static Char16Vector Char16VectorFromString16(const string16& uni_word);

  // Breaks the |uni_word| string down into its individual characters.
  // Note that this is temporarily intended to work on a single word, but
  // _will_ work on a string of words, perhaps with unexpected results.
  // TODO(mrossetti): Lots of optimizations possible here for not restarting
  // a search if the user is just typing along. Also, change this to uniString
  // and properly handle substring matches, scoring and sorting the results
  // by score. Also, provide the metrics for where the matches occur so that
  // the UI can highlight the matched sections.
  static Char16Set Char16SetFromString16(const string16& uni_word);

  // Given a single word in |uni_word|, adds a reference for the containing
  // history item identified by |history_id| to the index.
  void AddWordToIndex(const string16& uni_word, HistoryID history_id);

  // Updates an existing entry in the word/history index by adding the
  // |history_id| to set for |word_id| in the word_id_history_map_.
  void UpdateWordHistory(WordID word_id, HistoryID history_id);

  // Creates a new entry in the word/history map for |word_id| and add
  // |history_id| as the initial element of the word's set.
  void AddWordHistory(const string16& uni_word, HistoryID history_id);

  // Clears the search term cache. This cache holds on to the intermediate
  // word results for each previously typed character to eliminate the need
  // to re-perform set operations for previously typed characters.
  void ResetTermCharWordSetCache();

  // Composes a set of history item IDs by intersecting the set for each word
  // in |uni_string|.
  HistoryIDSet HistoryIDSetFromWords(const string16& uni_string);

  // Helper function to HistoryIDSetFromWords which composes a set of history
  // ids for the given term given in |uni_word|.
  HistoryIDSet HistoryIDsForTerm(const string16& uni_word);

  // Calculates a raw score for this history item by first determining
  // if all of the terms in |terms_vector| occur in |row| and, if so,
  // calculating a raw score based on 1) starting position of each term
  // in the user input, 2) completeness of each term's match, 3) ordering
  // of the occurrence of each term (i.e. they appear in order), 4) last
  // visit time, and 5) number of visits.
  // This raw score allows the results to be ordered and can be used
  // to influence the final score calculated by the client of this
  // index. Returns a ScoredHistoryMatch structure with the raw score and
  // substring matching metrics.
  static ScoredHistoryMatch ScoredMatchForURL(
      const URLRow& row,
      const String16Vector& terms_vector);

  // Calculates a partial raw score based on position, ordering and total
  // substring match size using metrics recorded in |matches|. |max_length|
  // is the length of the string against which the terms are being searched.
  static int RawScoreForMatches(const TermMatches& matches,
                                size_t max_length);

  // Sorts and removes overlapping substring matches from |matches| and
  // returns the cleaned up matches.
  static TermMatches SortAndDeoverlap(const TermMatches& matches);

  // Utility functions supporting RestoreFromCache and SaveToCache.

  // Construct a file path for the cache file within the same directory where
  // the history database is kept and saves that path to |file_path|. Returns
  // true if |file_path| can be successfully constructed. (This function
  // provided as a hook for unit testing.)
  bool GetCacheFilePath(FilePath* file_path);

  // Encode a data structure into the protobuf |cache|.
  void SavePrivateData(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordList(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveCharWordMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordIDHistoryMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveHistoryInfoMap(imui::InMemoryURLIndexCacheItem* cache) const;

  // Decode a data structure from the protobuf |cache|. Return false if there
  // is any kind of failure.
  bool RestorePrivateData(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordList(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreCharWordMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordIDHistoryMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreHistoryInfoMap(const imui::InMemoryURLIndexCacheItem& cache);

  // Directory where cache file resides. This is, except when unit testing,
  // the same directory in which the profile's history database is found. It
  // should never be empty.
  FilePath history_dir_;

  // The timestamp of when the cache was last saved. This is used to validate
  // the transaction journal's applicability to the cache. The timestamp is
  // initialized to the NULL time, indicating that the cache was not used with
  // the InMemoryURLIndex was last populated.
  base::Time last_saved_;

  // A list of all of indexed words. The index of a word in this list is the
  // ID of the word in the word_map_. It reduces the memory overhead by
  // replacing a potentially long and repeated string with a simple index.
  // NOTE: A word will _never_ be removed from this vector thus insuring
  // the immutability of the word_id throughout the session, reducing
  // maintenance complexity.
  // TODO(mrossetti): Profile the vector allocation and determine if judicious
  // 'reserve' calls are called for.
  String16Vector word_list_;

  int history_item_count_;
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
