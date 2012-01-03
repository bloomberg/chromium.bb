// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/history/url_index_private_data.h"
#include "sql/connection.h"

namespace base {
class Time;
}

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {

namespace imui = in_memory_url_index;

class URLDatabase;

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
  virtual ~InMemoryURLIndex();

  // Opens and indexes the URL history database. If the index private data
  // cannot be restored from its cache file then it is rebuilt from the
  // |history_db|. |languages| gives a list of language encodings by which URLs
  // and omnibox searches are broken down into words and characters.
  bool Init(URLDatabase* history_db, const std::string& languages);

  // Signals that any outstanding initialization should be canceled and
  // flushes the cache to disk.
  void ShutDown();

  // Reloads the history index from |history_db| ignoring any cache file that
  // may be available, clears the cache and saves the cache after reloading.
  bool ReloadFromHistory(history::URLDatabase* history_db);

  // Scans the history index and returns a vector with all scored, matching
  // history items. This entry point simply forwards the call on to the
  // URLIndexPrivateData class. For a complete description of this function
  // refer to that class.
  ScoredHistoryMatches HistoryItemsForTerms(const string16& term_string);

  // Updates or adds an history item to the index if it meets the minimum
  // 'quick' criteria.
  void UpdateURL(URLID row_id, const URLRow& row);

  // Deletes indexing data for an history item. The item may not have actually
  // been indexed (which is the case if it did not previously meet minimum
  // 'quick' criteria).
  void DeleteURL(URLID row_id);

 private:
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheFilePath);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, HugeResultSet);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleSearch);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TypedCharacterCaching);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, WhitelistedURLs);
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // Creating one of me without a history path is not allowed (tests excepted).
  InMemoryURLIndex();

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void ClearPrivateData();

  // Construct a file path for the cache file within the same directory where
  // the history database is kept and saves that path to |file_path|. Returns
  // true if |file_path| can be successfully constructed. (This function
  // provided as a hook for unit testing.)
  bool GetCacheFilePath(FilePath* file_path);

  // Directory where cache file resides. This is, except when unit testing,
  // the same directory in which the profile's history database is found. It
  // should never be empty.
  FilePath history_dir_;

  // The index's durable private data.
  scoped_ptr<URLIndexPrivateData> private_data_;

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
