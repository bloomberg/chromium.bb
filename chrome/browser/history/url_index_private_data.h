// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
#define CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/in_memory_url_index_cache.pb.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/scored_history_match.h"

class HistoryQuickProviderTest;

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {

namespace imui = in_memory_url_index;

class HistoryClient;
class HistoryDatabase;
class InMemoryURLIndex;
class RefCountedBool;

// Current version of the cache file.
static const int kCurrentCacheFileVersion = 5;

// A structure private to InMemoryURLIndex describing its internal data and
// providing for restoring, rebuilding and updating that internal data. As
// this class is for exclusive use by the InMemoryURLIndex class there should
// be no calls from any other class.
//
// All public member functions are called on the main thread unless otherwise
// annotated.
class URLIndexPrivateData
    : public base::RefCountedThreadSafe<URLIndexPrivateData> {
 public:
  URLIndexPrivateData();

  // Given a base::string16 in |term_string|, scans the history index and
  // returns a vector with all scored, matching history items. The
  // |term_string| is broken down into individual terms (words), each of which
  // must occur in the candidate history item's URL or page title for the item
  // to qualify; however, the terms do not necessarily have to be adjacent. We
  // also allow breaking |term_string| at |cursor_position| (if
  // set). Once we have a set of candidates, they are filtered to ensure
  // that all |term_string| terms, as separated by whitespace and the
  // cursor (if set), occur within the candidate's URL or page title.
  // Scores are then calculated on no more than |kItemsToScoreLimit|
  // candidates, as the scoring of such a large number of candidates may
  // cause perceptible typing response delays in the omnibox. This is
  // likely to occur for short omnibox terms such as 'h' and 'w' which
  // will be found in nearly all history candidates. Results are sorted by
  // descending score. The full results set (i.e. beyond the
  // |kItemsToScoreLimit| limit) will be retained and used for subsequent calls
  // to this function. |history_client| is used to boost a result's score if
  // its URL is referenced by one or more of the user's bookmarks.  |languages|
  // is used to help parse/format the URLs in the history index.  In total,
  // |max_matches| of items will be returned in the |ScoredHistoryMatches|
  // vector.
  ScoredHistoryMatches HistoryItemsForTerms(base::string16 term_string,
                                            size_t cursor_position,
                                            size_t max_matches,
                                            const std::string& languages,
                                            HistoryClient* history_client);

  // Adds the history item in |row| to the index if it does not already already
  // exist and it meets the minimum 'quick' criteria. If the row already exists
  // in the index then the index will be updated if the row still meets the
  // criteria, otherwise the row will be removed from the index. Returns true
  // if the index was actually updated. |languages| gives a list of language
  // encodings by which the URLs and page titles are broken down into words and
  // characters. |scheme_whitelist| is used to filter non-qualifying schemes.
  // |history_service| is used to schedule an update to the recent visits
  // component of this URL's entry in the index.
  bool UpdateURL(HistoryService* history_service,
                 const URLRow& row,
                 const std::string& languages,
                 const std::set<std::string>& scheme_whitelist,
                 base::CancelableTaskTracker* tracker);

  // Updates the entry for |url_id| in the index, replacing its
  // recent visits information with |recent_visits|.  If |url_id|
  // is not in the index, does nothing.
  void UpdateRecentVisits(URLID url_id,
                          const VisitVector& recent_visits);

  // Using |history_service| schedules an update (using the historyDB
  // thread) for the recent visits information for |url_id|.  Unless
  // something unexpectedly goes wrong, UdpateRecentVisits() should
  // eventually be called from a callback.
  void ScheduleUpdateRecentVisits(HistoryService* history_service,
                                  URLID url_id,
                                  base::CancelableTaskTracker* tracker);

  // Deletes index data for the history item with the given |url|.
  // The item may not have actually been indexed, which is the case if it did
  // not previously meet minimum 'quick' criteria. Returns true if the index
  // was actually updated.
  bool DeleteURL(const GURL& url);

  // Constructs a new object by restoring its contents from the cache file
  // at |path|. Returns the new URLIndexPrivateData which on success will
  // contain the restored data but upon failure will be empty.  |languages|
  // is used to break URLs and page titles into words.  This function
  // should be run on the the file thread.
  static scoped_refptr<URLIndexPrivateData> RestoreFromFile(
      const base::FilePath& path,
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

  // Writes |private_data| as a cache file to |file_path| and returns success.
  static bool WritePrivateDataToCacheFileTask(
      scoped_refptr<URLIndexPrivateData> private_data,
      const base::FilePath& file_path);

  // Creates a copy of ourself.
  scoped_refptr<URLIndexPrivateData> Duplicate() const;

  // Returns true if there is no data in the index.
  bool Empty() const;

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void Clear();

 private:
  friend class base::RefCountedThreadSafe<URLIndexPrivateData>;
  ~URLIndexPrivateData();

  friend class AddHistoryMatch;
  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, HugeResultSet);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, ReadVisitsFromHistory);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, RebuildFromHistoryIfCacheOld);
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
  typedef std::map<base::string16, SearchTermCacheItem> SearchTermCacheMap;

  // A helper class which performs the final filter on each candidate
  // history URL match, inserting accepted matches into |scored_matches_|.
  class AddHistoryMatch : public std::unary_function<HistoryID, void> {
   public:
    AddHistoryMatch(const URLIndexPrivateData& private_data,
                    const std::string& languages,
                    HistoryClient* history_client,
                    const base::string16& lower_string,
                    const String16Vector& lower_terms,
                    const base::Time now);
    ~AddHistoryMatch();

    void operator()(const HistoryID history_id);

    ScoredHistoryMatches ScoredMatches() const { return scored_matches_; }

   private:
    const URLIndexPrivateData& private_data_;
    const std::string& languages_;
    HistoryClient* history_client_;
    ScoredHistoryMatches scored_matches_;
    const base::string16& lower_string_;
    const String16Vector& lower_terms_;
    WordStarts lower_terms_to_word_starts_offsets_;
    const base::Time now_;
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

  // URL History indexing support functions.

  // Composes a set of history item IDs by intersecting the set for each word
  // in |unsorted_words|.
  HistoryIDSet HistoryIDSetFromWords(const String16Vector& unsorted_words);

  // Helper function to HistoryIDSetFromWords which composes a set of history
  // ids for the given term given in |term|.
  HistoryIDSet HistoryIDsForTerm(const base::string16& term);

  // Given a set of Char16s, finds words containing those characters.
  WordIDSet WordIDSetForTermChars(const Char16Set& term_chars);

  // Indexes one URL history item as described by |row|. Returns true if the
  // row was actually indexed. |languages| gives a list of language encodings by
  // which the URLs and page titles are broken down into words and characters.
  // |scheme_whitelist| is used to filter non-qualifying schemes.  If
  // |history_db| is not NULL then this function uses the history database
  // synchronously to get the URL's recent visits information.  This mode should
  // only be used on the historyDB thread.  If |history_db| is NULL, then
  // this function uses |history_service| to schedule a task on the
  // historyDB thread to fetch and update the recent visits
  // information.
  bool IndexRow(HistoryDatabase* history_db,
                HistoryService* history_service,
                const URLRow& row,
                const std::string& languages,
                const std::set<std::string>& scheme_whitelist,
                base::CancelableTaskTracker* tracker);

  // Parses and indexes the words in the URL and page title of |row| and
  // calculate the word starts in each, saving the starts in |word_starts|.
  // |languages| gives a list of language encodings by which the URLs and page
  // titles are broken down into words and characters.
  void AddRowWordsToIndex(const URLRow& row,
                          RowWordStarts* word_starts,
                          const std::string& languages);

  // Given a single word in |uni_word|, adds a reference for the containing
  // history item identified by |history_id| to the index.
  void AddWordToIndex(const base::string16& uni_word, HistoryID history_id);

  // Creates a new entry in the word/history map for |word_id| and add
  // |history_id| as the initial element of the word's set.
  void AddWordHistory(const base::string16& uni_word, HistoryID history_id);

  // Updates an existing entry in the word/history index by adding the
  // |history_id| to set for |word_id| in the word_id_history_map_.
  void UpdateWordHistory(WordID word_id, HistoryID history_id);

  // Adds |word_id| to |history_id|'s entry in the history/word map,
  // creating a new entry if one does not already exist.
  void AddToHistoryIDWordMap(HistoryID history_id, WordID word_id);

  // Removes |row| and all associated words and characters from the index.
  void RemoveRowFromIndex(const URLRow& row);

  // Removes all words and characters associated with |row| from the index.
  void RemoveRowWordsFromIndex(const URLRow& row);

  // Clears |used_| for each item in the search term cache.
  void ResetSearchTermCache();

  // Caches the index private data and writes the cache file to the profile
  // directory.  Called by WritePrivateDataToCacheFileTask.
  bool SaveToFile(const base::FilePath& file_path);

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

  // The last time the data was rebuilt from the history database.
  base::Time last_time_rebuilt_from_history_;

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
