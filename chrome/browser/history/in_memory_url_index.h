// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/scored_history_match.h"
#include "components/history/core/browser/history_types.h"
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

class HistoryClient;
class HistoryDatabase;
class URLIndexPrivateData;
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
class InMemoryURLIndex : public content::NotificationObserver,
                         public base::SupportsWeakPtr<InMemoryURLIndex> {
 public:
  // Defines an abstract class which is notified upon completion of restoring
  // the index's private data either by reading from the cache file or by
  // rebuilding from the history database.
  class RestoreCacheObserver {
   public:
    virtual ~RestoreCacheObserver();

    // Callback that lets the observer know that the restore operation has
    // completed. |succeeded| indicates if the restore was successful. This is
    // called on the UI thread.
    virtual void OnCacheRestoreFinished(bool succeeded) = 0;
  };

  // Defines an abstract class which is notified upon completion of saving
  // the index's private data to the cache file.
  class SaveCacheObserver {
   public:
    virtual ~SaveCacheObserver();

    // Callback that lets the observer know that the save succeeded.
    // This is called on the UI thread.
    virtual void OnCacheSaveFinished(bool succeeded) = 0;
  };

  // |profile|, which may be NULL during unit testing, is used to register for
  // history changes. |history_dir| is a path to the directory containing the
  // history database within the profile wherein the cache and transaction
  // journals will be stored. |languages| gives a list of language encodings by
  // which URLs and omnibox searches are broken down into words and characters.
  InMemoryURLIndex(Profile* profile,
                   const base::FilePath& history_dir,
                   const std::string& languages,
                   HistoryClient* client);
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
  // refer to that class.  If |cursor_position| is base::string16::npos, the
  // function doesn't do anything special with the cursor; this is equivalent
  // to the cursor being at the end.  In total, |max_matches| of items will be
  // returned in the |ScoredHistoryMatches| vector.
  ScoredHistoryMatches HistoryItemsForTerms(const base::string16& term_string,
                                            size_t cursor_position,
                                            size_t max_matches);

  // Deletes the index entry, if any, for the given |url|.
  void DeleteURL(const GURL& url);

  // Sets the optional observers for completion of restoral and saving of the
  // index's private data.
  void set_restore_cache_observer(
      RestoreCacheObserver* restore_cache_observer) {
    restore_cache_observer_ = restore_cache_observer;
  }
  void set_save_cache_observer(SaveCacheObserver* save_cache_observer) {
    save_cache_observer_ = save_cache_observer;
  }

  // Indicates that the index restoration is complete.
  bool restored() const {
    return restored_;
  }

 private:
  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndexTest;
  friend class InMemoryURLIndexCacheTest;
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // Creating one of me without a history path is not allowed (tests excepted).
  InMemoryURLIndex();

  // HistoryDBTask used to rebuild our private data from the history database.
  class RebuildPrivateDataFromHistoryDBTask : public HistoryDBTask {
   public:
    explicit RebuildPrivateDataFromHistoryDBTask(
        InMemoryURLIndex* index,
        const std::string& languages,
        const std::set<std::string>& scheme_whitelist);

    virtual bool RunOnDBThread(HistoryBackend* backend,
                               history::HistoryDatabase* db) OVERRIDE;
    virtual void DoneRunOnMainThread() OVERRIDE;

   private:
    virtual ~RebuildPrivateDataFromHistoryDBTask();

    InMemoryURLIndex* index_;  // Call back to this index at completion.
    std::string languages_;  // Languages for word-breaking.
    std::set<std::string> scheme_whitelist_;  // Schemes to be indexed.
    bool succeeded_;  // Indicates if the rebuild was successful.
    scoped_refptr<URLIndexPrivateData> data_;  // The rebuilt private data.

    DISALLOW_COPY_AND_ASSIGN(RebuildPrivateDataFromHistoryDBTask);
  };

  // Initializes all index data members in preparation for restoring the index
  // from the cache or a complete rebuild from the history database.
  void ClearPrivateData();

  // Constructs a file path for the cache file within the same directory where
  // the history database is kept and saves that path to |file_path|. Returns
  // true if |file_path| can be successfully constructed. (This function
  // provided as a hook for unit testing.)
  bool GetCacheFilePath(base::FilePath* file_path);

  // Restores the index's private data from the cache file stored in the
  // profile directory.
  void PostRestoreFromCacheFileTask();

  // Schedules a history task to rebuild our private data from the history
  // database.
  void ScheduleRebuildFromHistory();

  // Callback used by RebuildPrivateDataFromHistoryDBTask to signal completion
  // or rebuilding our private data from the history database. |succeeded|
  // will be true if the rebuild was successful. |data| will point to a new
  // instanceof the private data just rebuilt.
  void DoneRebuidingPrivateDataFromHistoryDB(
      bool succeeded,
      scoped_refptr<URLIndexPrivateData> private_data);

  // Rebuilds the history index from the history database in |history_db|.
  // Used for unit testing only.
  void RebuildFromHistory(HistoryDatabase* history_db);

  // Determines if the private data was successfully reloaded from the cache
  // file or if the private data must be rebuilt from the history database.
  // |private_data_ptr|'s data will be NULL if the cache file load failed. If
  // successful, sets the private data and notifies any
  // |restore_cache_observer_|. Otherwise, kicks off a rebuild from the history
  // database.
  void OnCacheLoadDone(scoped_refptr<URLIndexPrivateData> private_data_ptr);

  // Callback function that sets the private data from the just-restored-from-
  // file |private_data|. Notifies any |restore_cache_observer_| that the
  // restore has succeeded.
  void OnCacheRestored(URLIndexPrivateData* private_data);

  // Posts a task to cache the index private data and write the cache file to
  // the profile directory.
  void PostSaveToCacheFileTask();

  // Saves private_data_ to the given |path|. Runs on the UI thread.
  // Provided for unit testing so that a test cache file can be used.
  void DoSaveToCacheFile(const base::FilePath& path);

  // Notifies the observer, if any, of the success of the private data caching.
  // |succeeded| is true on a successful save.
  void OnCacheSaveDone(bool succeeded);

  // Handles notifications of history changes.
  virtual void Observe(int notification_type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Notification handlers.
  void OnURLVisited(const URLVisitedDetails* details);
  void OnURLsModified(const URLsModifiedDetails* details);
  void OnURLsDeleted(const URLsDeletedDetails* details);

  // Sets the directory wherein the cache file will be maintained.
  // For unit test usage only.
  void set_history_dir(const base::FilePath& dir_path) {
    history_dir_ = dir_path;
  }

  // Returns a pointer to our private data. For unit testing only.
  URLIndexPrivateData* private_data() { return private_data_.get(); }

  // Returns a pointer to our private data cancelable request tracker. For
  // unit testing only.
  base::CancelableTaskTracker* private_data_tracker() {
    return &private_data_tracker_;
  }

  // Returns the set of whitelisted schemes. For unit testing only.
  const std::set<std::string>& scheme_whitelist() { return scheme_whitelist_; }

  // The profile, may be null when testing.
  Profile* profile_;

  // The HistoryClient; may be NULL when testing.
  HistoryClient* history_client_;

  // Directory where cache file resides. This is, except when unit testing,
  // the same directory in which the profile's history database is found. It
  // should never be empty.
  base::FilePath history_dir_;

  // Languages used during the word-breaking process during indexing.
  std::string languages_;

  // Only URLs with a whitelisted scheme are indexed.
  std::set<std::string> scheme_whitelist_;

  // The index's durable private data.
  scoped_refptr<URLIndexPrivateData> private_data_;

  // Observers to notify upon restoral or save of the private data cache.
  RestoreCacheObserver* restore_cache_observer_;
  SaveCacheObserver* save_cache_observer_;

  base::CancelableTaskTracker private_data_tracker_;
  base::CancelableTaskTracker cache_reader_tracker_;
  content::NotificationRegistrar registrar_;

  // Set to true once the shutdown process has begun.
  bool shutdown_;

  // Set to true once the index restoration is complete.
  bool restored_;

  // Set to true when changes to the index have been made and the index needs
  // to be cached. Set to false when the index has been cached. Used as a
  // temporary safety check to insure that the cache is saved before the
  // index has been destructed.
  bool needs_to_be_cached_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLIndex);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
