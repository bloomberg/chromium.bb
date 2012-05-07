// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
#define CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
#pragma once

#include <set>
#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/in_memory_url_index_cache.pb.h"
#include "content/public/browser/notification_details.h"

class HistoryQuickProviderTest;

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {

namespace imui = in_memory_url_index;

class HistoryDatabase;
class InMemoryURLIndex;
class RefCountedBool;

// Current version of the cache file.
static const int kCurrentCacheFileVersion = 1;

// A structure describing the InMemoryURLIndex's internal data and providing for
// restoring, rebuilding and updating that internal data.
class URLIndexPrivateData
    : public base::RefCountedThreadSafe<URLIndexPrivateData> {
 public:
  URLIndexPrivateData();

 private:
  friend class base::RefCountedThreadSafe<URLIndexPrivateData>;
  ~URLIndexPrivateData();

  friend class AddHistoryMatch;
  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndex;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, HugeResultSet);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, Scoring);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleSearch);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TypedCharacterCaching);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, WhitelistedURLs);
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // Support caching of term results so that we can optimize searches which
  // build upon a previous search. Each entry in this map represents one
  // search term from the most recent search. For example, if the user had
  // typed "google blog trans" and then typed an additional 'l' (at the end,
  // of course) then there would be four items in the cache: 'blog', 'google',
  // 'trans', and 'transl'. All would be marked as being in use except for the
  // 'trans' item; its cached data would have been used when optimizing the
  // construction of the search results candidates for 'transl' but then would
  // no longer needed.
  //
  // Items stored in the search term cache. If a search term exactly matches one
  // in the cache then we can quickly supply the proper |history_id_set_| (and
  // marking the cache item as being |used_|. If we find a prefix for a search
  // term in the cache (which is very likely to occur as the user types each
  // term into the omnibox) then we can short-circuit the index search for those
  // characters in the prefix by returning the |word_id_set|. In that case we do
  // not mark the item as being |used_|.
  struct SearchTermCacheItem {
    SearchTermCacheItem(const WordIDSet& word_id_set,
                        const HistoryIDSet& history_id_set);
    // Creates a cache item for a term which has no results.
    SearchTermCacheItem();

    ~SearchTermCacheItem();

    WordIDSet word_id_set_;
    HistoryIDSet history_id_set_;
    bool used_;  // True if this item has been used for the current term search.
  };
  typedef std::map<string16, SearchTermCacheItem> SearchTermCacheMap;

  // A helper class which performs the final filter on each candidate
  // history URL match, inserting accepted matches into |scored_matches_|
  // and trimming the maximum number of matches to 10.
  class AddHistoryMatch : public std::unary_function<HistoryID, void> {
   public:
    AddHistoryMatch(const URLIndexPrivateData& private_data,
                    const string16& lower_string,
                    const String16Vector& lower_terms);
    ~AddHistoryMatch();

    void operator()(const HistoryID history_id);

    ScoredHistoryMatches ScoredMatches() const { return scored_matches_; }

   private:
    const URLIndexPrivateData& private_data_;
    ScoredHistoryMatches scored_matches_;
    const string16& lower_string_;
    const String16Vector& lower_terms_;
  };

  // A helper predicate class used to filter excess history items when the
  // candidate results set is too large.
  class HistoryItemFactorGreater
      : public std::binary_function<HistoryID, HistoryID, void> {
   public:
    explicit HistoryItemFactorGreater(const HistoryInfoMap& history_info_map);
    ~HistoryItemFactorGreater();

    bool operator()(const HistoryID h1, const HistoryID h2);

   private:
    const history::HistoryInfoMap& history_info_map_;
  };

  // Given a string16 in |term_string|, scans the history index and returns a
  // vector with all scored, matching history items. The |term_string| is
  // broken down into individual terms (words), each of which must occur in the
  // candidate history item's URL or page title for the item to qualify;
  // however, the terms do not necessarily have to be adjacent. Once we have
  // a set of candidates, they are filtered to insure that all |term_string|
  // terms, as separated by whitespace, occur within the candidate's URL
  // or page title. Scores are then calculated on no more than
  // |kItemsToScoreLimit| candidates, as the scoring of such a large number of
  // candidates may cause perceptible typing response delays in the omnibox.
  // This is likely to occur for short omnibox terms such as 'h' and 'w' which
  // will be found in nearly all history candidates. Results are sorted by
  // descending score. The full results set (i.e. beyond the
  // |kItemsToScoreLimit| limit) will be retained and used for subsequent calls
  // to this function.
  ScoredHistoryMatches HistoryItemsForTerms(const string16& term_string);

  // Creates a new URLIndexPrivateData object, populates it from the contents
  // of the cache file stored in |file_path|, and assigns it to |private_data|.
  // |languages| will be used to break URLs and page titles into words.
  static void RestoreFromFileTask(
      const FilePath& file_path,
      scoped_refptr<URLIndexPrivateData> private_data,
      const std::string& languages);

  // Constructs a new object by restoring its contents from the file at |path|.
  // Returns the new URLIndexPrivateData which on success will contain the
  // restored data but upon failure will be empty.  |languages| will be used to
  // break URLs and page titles into words
  static scoped_refptr<URLIndexPrivateData> RestoreFromFile(
      const FilePath& path,
      const std::string& languages);

  // Constructs a new object by rebuilding its contents from the history
  // database in |history_db|. Returns the new URLIndexPrivateData which on
  // success will contain the rebuilt data but upon failure will be empty.
  // |languages| gives a list of language encodings by which the URLs and page
  // titles are broken down into words and characters.
  static scoped_refptr<URLIndexPrivateData> RebuildFromHistory(
      HistoryDatabase* history_db,
      const std::string& languages,
      const std::set<std::string>& scheme_whitelist);

  // Writes |private_data| as a cache file to |file_path| and returns success
  // via |succeeded|.
  static void WritePrivateDataToCacheFileTask(
      scoped_refptr<URLIndexPrivateData> private_data,
      const FilePath& file_path,
      scoped_refptr<RefCountedBool> succeeded);

  // Caches the index private data and writes the cache file to the profile
  // directory.  Called by WritePrivateDataToCacheFileTask.
  bool SaveToFile(const FilePath& file_path);

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void Clear();

  // Returns true if there is no data in the index.
  bool Empty() const;

  // Creates a copy of ourself.
  scoped_refptr<URLIndexPrivateData> Duplicate() const;

  // Adds |word_id| to |history_id|'s entry in the history/word map,
  // creating a new entry if one does not already exist.
  void AddToHistoryIDWordMap(HistoryID history_id, WordID word_id);

  // Given a set of Char16s, finds words containing those characters.
  WordIDSet WordIDSetForTermChars(const Char16Set& term_chars);

  // URL History indexing support functions.

  // Indexes one URL history item as described by |row|. Returns true if the
  // row was actually indexed. |languages| gives a list of language encodings by
  // which the URLs and page titles are broken down into words and characters.
  // |scheme_whitelist| is used to filter non-qualifying schemes.
  bool IndexRow(const URLRow& row,
                const std::string& languages,
                const std::set<std::string>& scheme_whitelist);

  // Adds the history item in |row| to the index if it does not already already
  // exist and it meets the minimum 'quick' criteria. If the row already exists
  // in the index then the index will be updated if the row still meets the
  // criteria, otherwise the row will be removed from the index. Returns true
  // if the index was actually updated. |languages| gives a list of language
  // encodings by which the URLs and page titles are broken down into words and
  // characters. |scheme_whitelist| is used to filter non-qualifying schemes.
  bool UpdateURL(const URLRow& row,
                 const std::string& languages,
                 const std::set<std::string>& scheme_whitelist);

  // Deletes indexing data for the history item with the URL given in |url|.
  // The item may not have actually been indexed, which is the case if it did
  // not previously meet minimum 'quick' criteria. Returns true if the index
  // was actually updated.
  bool DeleteURL(const GURL& url);

  // Parses and indexes the words in the URL and page title of |row| and
  // calculate the word starts in each, saving the starts in |word_starts|.
  // |languages| gives a list of language encodings by which the URLs and page
  // titles are broken down into words and characters.
  void AddRowWordsToIndex(const URLRow& row,
                          RowWordStarts* word_starts,
                          const std::string& languages);

  // Removes |row| and all associated words and characters from the index.
  void RemoveRowFromIndex(const URLRow& row);

  // Removes all words and characters associated with |row| from the index.
  void RemoveRowWordsFromIndex(const URLRow& row);

  // Given a single word in |uni_word|, adds a reference for the containing
  // history item identified by |history_id| to the index.
  void AddWordToIndex(const string16& uni_word, HistoryID history_id);

  // Updates an existing entry in the word/history index by adding the
  // |history_id| to set for |word_id| in the word_id_history_map_.
  void UpdateWordHistory(WordID word_id, HistoryID history_id);

  // Creates a new entry in the word/history map for |word_id| and add
  // |history_id| as the initial element of the word's set.
  void AddWordHistory(const string16& uni_word, HistoryID history_id);

  // Clears |used_| for each item in the search term cache.
  void ResetSearchTermCache();

  // Composes a set of history item IDs by intersecting the set for each word
  // in |unsorted_words|.
  HistoryIDSet HistoryIDSetFromWords(const String16Vector& unsorted_words);

  // Helper function to HistoryIDSetFromWords which composes a set of history
  // ids for the given term given in |term|.
  HistoryIDSet HistoryIDsForTerm(const string16& term);

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
      const string16& lower_string,
      const String16Vector& terms_vector,
      const RowWordStarts& word_starts);

  // Calculates a component score based on position, ordering and total
  // substring match size using metrics recorded in |matches|. |max_length|
  // is the length of the string against which the terms are being searched.
  static int ScoreComponentForMatches(const TermMatches& matches,
                                      size_t max_length);

  // Encode a data structure into the protobuf |cache|.
  void SavePrivateData(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordList(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveCharWordMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordIDHistoryMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveHistoryInfoMap(imui::InMemoryURLIndexCacheItem* cache) const;
  void SaveWordStartsMap(imui::InMemoryURLIndexCacheItem* cache) const;

  // Decode a data structure from the protobuf |cache|. Return false if there
  // is any kind of failure. |languages| will be used to break URLs and page
  // titles into words
  bool RestorePrivateData(const imui::InMemoryURLIndexCacheItem& cache,
                          const std::string& languages);
  bool RestoreWordList(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreCharWordMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordIDHistoryMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreHistoryInfoMap(const imui::InMemoryURLIndexCacheItem& cache);
  bool RestoreWordStartsMap(const imui::InMemoryURLIndexCacheItem& cache,
                            const std::string& languages);

  // Determines if |gurl| has a whitelisted scheme and returns true if so.
  static bool URLSchemeIsWhitelisted(const GURL& gurl,
                                     const std::set<std::string>& whitelist);

  // Cache of search terms.
  SearchTermCacheMap search_term_cache_;

  // Start of data members that are cached -------------------------------------

  // The version of the cache file most recently used to restore this instance
  // of the private data. If the private data was rebuilt from the history
  // database this will be 0.
  int restored_cache_version_;

  // A list of all of indexed words. The index of a word in this list is the
  // ID of the word in the word_map_. It reduces the memory overhead by
  // replacing a potentially long and repeated string with a simple index.
  String16Vector word_list_;

  // A list of available words slots in |word_list_|. An available word slot
  // is the index of a unused word in word_list_ vector, also referred to as
  // a WordID. As URL visits are added or modified new words may be added to
  // the index, in which case any available words are used, if any, and then
  // words are added to the end of the word_list_. When URL visits are
  // modified or deleted old words may be removed from the index, in which
  // case the slots for those words are added to available_words_ for resuse
  // by future URL updates.
  WordIDSet available_words_;

  // A one-to-one mapping from the a word string to its slot number (i.e.
  // WordID) in the |word_list_|.
  WordMap word_map_;

  // A one-to-many mapping from a single character to all WordIDs of words
  // containing that character.
  CharWordIDMap char_word_map_;

  // A one-to-many mapping from a WordID to all HistoryIDs (the row_id as
  // used in the history database) of history items in which the word occurs.
  WordIDHistoryMap word_id_history_map_;

  // A one-to-many mapping from a HistoryID to all WordIDs of words that occur
  // in the URL and/or page title of the history item referenced by that
  // HistoryID.
  HistoryIDWordMap history_id_word_map_;

  // A one-to-one mapping from HistoryID to the history item data governing
  // index inclusion and relevance scoring.
  HistoryInfoMap history_info_map_;

  // A one-to-one mapping from HistoryID to the word starts detected in each
  // item's URL and page title.
  WordStartsMap word_starts_map_;

  // End of data members that are cached ---------------------------------------

  // For unit testing only. Specifies the version of the cache file to be saved.
  // Used only for testing upgrading of an older version of the cache upon
  // restore.
  int saved_cache_version_;

  // Used for unit testing only. Records the number of candidate history items
  // at three stages in the index searching process.
  size_t pre_filter_item_count_;    // After word index is queried.
  size_t post_filter_item_count_;   // After trimming large result set.
  size_t post_scoring_item_count_;  // After performing final filter/scoring.
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
