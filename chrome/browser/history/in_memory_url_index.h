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

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_provider_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/in_memory_url_index_cache.pb.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class Profile;

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {

namespace imui = in_memory_url_index;

class URLDatabase;
struct URLsDeletedDetails;
struct URLsModifiedDetails;
struct URLVisitedDetails;

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
class InMemoryURLIndex : public content::NotificationObserver {
 public:
  // |history_dir| is a path to the directory containing the history database
  // within the profile wherein the cache and transaction journals will be
  // stored.
  InMemoryURLIndex(Profile* profile, const FilePath& history_dir);
  virtual ~InMemoryURLIndex();

  // Restores our index from its cache, if possible. If the cache is not
  // available then we will register for the NOTIFICATION_HISTORY_LOADED
  // notifications and then rebuild the index from the history database.
  // |languages| gives a list of language encodings with which the history
  // URLs and omnibox searches are interpreted, i.e. when each is broken
  // down into words and each word is broken down into characters.
  void Init(const std::string& languages);

  // Reloads the history index from the history database given in |history_db|.
  void ReloadFromHistory(URLDatabase* history_db);

  // Signals that any outstanding initialization should be canceled and
  // flushes the cache to disk.
  void ShutDown();

  // Caches the index private data and writes the cache file to the profile
  // directory.
  bool SaveToCacheFile();

  // Given a vector containing one or more words as string16s, scans the
  // history index and return a vector with all scored, matching history items.
  // Each term must occur somewhere in the history item's URL or page title for
  // the item to qualify; however, the terms do not necessarily have to be
  // adjacent. Results are sorted with higher scoring items first. Each term
  // from |terms| may contain punctuation but should not contain spaces.
  // A search request which results in more than |kItemsToScoreLimit| total
  // candidate items returns no matches (though the results set will be
  // retained and used for subsequent calls to this function) as the scoring
  // of such a large number of candidates may cause perceptible typing response
  // delays in the omnibox. This is likely to occur for short omnibox terms
  // such as 'h' and 'w' which will be found in nearly all history candidates.
  ScoredHistoryMatches HistoryItemsForTerms(const String16Vector& terms);

  // Updates or adds an history item to the index if it meets the minimum
  // selection criteria.
  void UpdateURL(const URLRow& row);

  // Deletes indexing data for an history item. The item may not have actually
  // been indexed (which is the case if it did not previously meet minimum
  // 'quick' criteria).
  void DeleteURL(const URLRow& row);

  // Notification callback.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  friend class AddHistoryMatch;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheFilePath);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, Char16Utilities);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, NonUniqueTermCharacterSets);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, Scoring);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, StaticFunctions);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleSearch);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleChange);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TypedCharacterCaching);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, WhitelistedURLs);

  // Signals that there are no previously cached results for the typed term.
  static const size_t kNoCachedResultForTerm;

  // Creating one of me without a history path is not allowed (tests excepted).
  InMemoryURLIndex();

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

  // Initialization and Restoration --------------------------------------------

  // Restores the index's private data from the cache, if possible, otherwise
  // registers to be notified when the history database becomes available.
  void RestoreFromCache();

  // Restores the index's private data from the cache file stored in the
  // profile directory and returns true if successful.
  bool RestoreFromCacheFile();

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void ClearPrivateData();

  // Initializes the whitelist of URL schemes.
  static void InitializeSchemeWhitelist(std::set<std::string>* whitelist);

  // URL History Indexing ------------------------------------------------------

  // Indexes one URL history item.
  void IndexRow(const URLRow& row);

  // Parses and indexes the words in the URL and page title of |row|.
  void AddRowWordsToIndex(const URLRow& row);

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
  // in |uni_string|.
  HistoryIDSet HistoryIDSetFromWords(const string16& uni_string);

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
      const String16Vector& terms_vector);

  // Calculates a component score based on position, ordering and total
  // substring match size using metrics recorded in |matches|. |max_length|
  // is the length of the string against which the terms are being searched.
  static int ScoreComponentForMatches(const TermMatches& matches,
                                      size_t max_length);

  // Determines if |gurl| has a whitelisted scheme and returns true if so.
  bool URLSchemeIsWhitelisted(const GURL& gurl) const;

  // Notification handlers.
  void OnURLVisited(const URLVisitedDetails* details);
  void OnURLsModified(const URLsModifiedDetails* details);
  void OnURLsDeleted(const URLsDeletedDetails* details);

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

  content::NotificationRegistrar registrar_;

  // The profile with which we are associated.
  Profile* profile_;

  // Directory where cache file resides. This is, except when unit testing,
  // the same directory in which the profile's history database is found. It
  // should never be empty.
  FilePath history_dir_;

  // The timestamp of when the cache was last saved. This is used to validate
  // the transaction journal's applicability to the cache. The timestamp is
  // initialized to the NULL time, indicating that the cache was not used with
  // the InMemoryURLIndex was last populated.
  base::Time last_saved_;

  // The index's durable private data.
  scoped_ptr<URLIndexPrivateData> private_data_;

  // Cache of search terms.
  SearchTermCacheMap search_term_cache_;

  // Languages used during the word-breaking process during indexing.
  std::string languages_;

  // Only URLs with a whitelisted scheme are indexed.
  std::set<std::string> scheme_whitelist_;

  // Set to true at shutdown when the cache has been written to disk. Used
  // as a temporary safety check to insure that the cache is saved before
  // the index has been destructed.
  // TODO(mrossetti): Eliminate once the transition to SQLite has been done.
  // http://crbug.com/83659
  bool cached_at_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLIndex);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
