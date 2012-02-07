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
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/in_memory_url_index_cache.pb.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sql/connection.h"

class HistoryQuickProviderTest;
class Profile;

namespace base {
class Time;
}

namespace in_memory_url_index {
class InMemoryURLIndexCacheItem;
}

namespace history {

namespace imui = in_memory_url_index;

class HistoryDatabase;
class URLIndexPrivateData;
struct URLVisitedDetails;
struct URLsModifiedDetails;
struct URLsDeletedDetails;

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
  // |profile|, which may be NULL during unit testing, is used to register for
  // history changes. |history_dir| is a path to the directory containing the
  // history database within the profile wherein the cache and transaction
  // journals will be stored. |languages| gives a list of language encodings by
  // which URLs and omnibox searches are broken down into words and characters.
  InMemoryURLIndex(Profile* profile,
                   const FilePath& history_dir,
                   const std::string& languages);
  virtual ~InMemoryURLIndex();

  // Opens and prepares the index of historical URL visits. If the index private
  // data cannot be restored from its cache file then it is rebuilt from the
  // history database.
  void Init();

  // Signals that any outstanding initialization should be canceled and
  // flushes the cache to disk.
  void ShutDown();

  // Scans the history index and returns a vector with all scored, matching
  // history items. This entry point simply forwards the call on to the
  // URLIndexPrivateData class. For a complete description of this function
  // refer to that class.
  ScoredHistoryMatches HistoryItemsForTerms(const string16& term_string);

 private:
  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexCacheTest, CacheFilePath);

  // Creating one of me without a history path is not allowed (tests excepted).
  InMemoryURLIndex();

  // HistoryDBTask used to rebuild our private data from the history database.
  class RebuildPrivateDataFromHistoryDBTask : public HistoryDBTask {
   public:
    explicit RebuildPrivateDataFromHistoryDBTask(InMemoryURLIndex* index);
    virtual ~RebuildPrivateDataFromHistoryDBTask();

    virtual bool RunOnDBThread(HistoryBackend* backend,
                               history::HistoryDatabase* db) OVERRIDE;
    virtual void DoneRunOnMainThread() OVERRIDE;

   private:
    InMemoryURLIndex* index_;  // Call back to this index at completion.
    bool succeeded_;  // Indicates if the rebuild was successful.
    scoped_ptr<URLIndexPrivateData> data_;  // The rebuilt private data.

    DISALLOW_COPY_AND_ASSIGN(RebuildPrivateDataFromHistoryDBTask);
  };

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void ClearPrivateData();

  // Constructs a file path for the cache file within the same directory where
  // the history database is kept and saves that path to |file_path|. Returns
  // true if |file_path| can be successfully constructed. (This function
  // provided as a hook for unit testing.)
  bool GetCacheFilePath(FilePath* file_path);

  // Restores the index's private data from the cache file stored in the
  // profile directory.
  void RestoreFromCacheFile();

  // Restores private_data_ from the given |path|. Runs on the UI thread.
  // Provided for unit testing so that a test cache file can be used.
  void DoRestoreFromCacheFile(const FilePath& path);

  // Schedules a history task to rebuild our private data from the history
  // database.
  void ScheduleRebuildFromHistory();

  // Callback used by RebuildPrivateDataFromHistoryDBTask to signal completion
  // or rebuilding our private data from the history database. |data| points to
  // a new instance of the private data just rebuilt. This callback is only
  // called upon a successful restore from the history database.
  void DoneRebuidingPrivateDataFromHistoryDB(URLIndexPrivateData* data);

  // Rebuilds the history index from the history database in |history_db|.
  // Used for unit testing only.
  void RebuildFromHistory(HistoryDatabase* history_db);

  // Caches the index private data and writes the cache file to the profile
  // directory.
  void SaveToCacheFile();

  // Saves private_data_ to the given |path|. Runs on the UI thread.
  // Provided for unit testing so that a test cache file can be used.
  void DoSaveToCacheFile(const FilePath& path);

  // Handles notifications of history changes.
  virtual void Observe(int notification_type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Notification handlers.
  void OnURLVisited(const URLVisitedDetails* details);
  void OnURLsModified(const URLsModifiedDetails* details);
  void OnURLsDeleted(const URLsDeletedDetails* details);

  // Returns a pointer to our private data. For unit testing only.
  URLIndexPrivateData* private_data() { return private_data_.get(); }

  // The profile, may be null when testing.
  Profile* profile_;

  // Directory where cache file resides. This is, except when unit testing,
  // the same directory in which the profile's history database is found. It
  // should never be empty.
  FilePath history_dir_;

  // The index's durable private data.
  scoped_ptr<URLIndexPrivateData> private_data_;

  // Set to true once the shutdown process has begun.
  bool shutdown_;

  CancelableRequestConsumer cache_reader_consumer_;
  content::NotificationRegistrar registrar_;

  // Set to true when changes to the index have been made and the index needs
  // to be cached. Set to false when the index has been cached. Used as a
  // temporary safety check to insure that the cache is saved before the
  // index has been destructed.
  // TODO(mrossetti): Eliminate once the transition to SQLite has been done.
  // http://crbug.com/83659
  bool needs_to_be_cached_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLIndex);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
