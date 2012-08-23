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
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/history_provider_util.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/scored_history_match.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sql/connection.h"

class HistoryQuickProviderTest;
class Profile;

namespace base {
class Time;
}

namespace history {

class InMemoryURLIndexObserver;
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
  // Observer is used for blocking until the InMemoryURLIndex has finished
  // loading. Usage typically follows this pattern:
  //    InMemoryURLIndex::Observer observer(index); // Create observer.
  //    MessageLoop::current()->Run();              // Blocks until loaded.
  //
  class Observer {
   public:
    explicit Observer(InMemoryURLIndex* index);

    // Called when the InMemoryURLIndex has completed loading.
    virtual void Loaded();

   private:
    friend class InMemoryURLIndexBaseTest;
    virtual ~Observer();

    InMemoryURLIndex* index_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

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
  // data cannot be restored from its cache database then it is rebuilt from the
  // history database. |disable_cache| causes the InMemoryURLIndex to not create
  // or use its cache database.
  void Init(bool disable_cache);

  // Signals that any outstanding initialization should be canceled.
  void Shutdown();

  // Returns true if the index has been loaded or rebuilt and so is available
  // for use.
  bool index_available() const { return index_available_; }

  // Scans the history index and returns a vector with all scored, matching
  // history items. This entry point simply forwards the call on to the
  // URLIndexPrivateData class. For a complete description of this function
  // refer to that class.
  ScoredHistoryMatches HistoryItemsForTerms(const string16& term_string);

  // Deletes the index entry, if any, for the given |url|.
  void DeleteURL(const GURL& url);

 private:
  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndex::Observer;
  friend class InMemoryURLIndexCacheTest;
  friend class InMemoryURLIndexTest;
  friend class InMemoryURLIndexBaseTest;
  friend class IntercessionaryIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, ExpireRow);
  FRIEND_TEST_ALL_PREFIXES(IntercessionaryIndexTest, CacheDatabaseFailure);
  FRIEND_TEST_ALL_PREFIXES(IntercessionaryIndexTest,
                           ShutdownDuringCacheRefresh);
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // HistoryDBTask used to rebuild our private data from the history database.
  class RebuildPrivateDataFromHistoryDBTask : public HistoryDBTask {
   public:
    explicit RebuildPrivateDataFromHistoryDBTask(InMemoryURLIndex* index);

    virtual bool RunOnDBThread(HistoryBackend* backend,
                               history::HistoryDatabase* db) OVERRIDE;
    virtual void DoneRunOnMainThread() OVERRIDE;

   private:
    virtual ~RebuildPrivateDataFromHistoryDBTask();

    InMemoryURLIndex* index_;  // Call back to this index at completion.
    bool succeeded_;  // Indicates if the rebuild was successful.
    scoped_refptr<URLIndexPrivateData> data_;  // The rebuilt private data.

    DISALLOW_COPY_AND_ASSIGN(RebuildPrivateDataFromHistoryDBTask);
  };

  // For unit testing only.
  InMemoryURLIndex(const FilePath& history_dir, const std::string& languages);

  // Completes index initialization once the cache DB has been initialized.
  void OnPrivateDataInitDone(bool succeeded);

  // Adds or removes an observer that is notified when the index has been
  // loaded.
  void AddObserver(InMemoryURLIndex::Observer* observer);
  void RemoveObserver(InMemoryURLIndex::Observer* observer);

  // Handles notifications of history changes.
  virtual void Observe(int notification_type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Notification handlers.
  void OnURLVisited(const URLVisitedDetails* details);
  void OnURLsModified(const URLsModifiedDetails* details);
  void OnURLsDeleted(const URLsDeletedDetails* details);

  // Posts any outstanding updates to the index which were queued while the
  // index was being initialized.
  void FlushPendingUpdates();

  // Restores the index's private data from the cache database stored in the
  // profile directory. If no database is found or can be restored then look
  // for an old version protobuf-based cache file.
  void PostRestoreFromCacheTask();

  // Determines if the private data was successfully restored from the cache
  // database, as indicated by |succeeded|, or if the private data must be
  // rebuilt from the history database. If successful, notifies any
  // |restore_cache_observer_|. Otherwise, kicks off a rebuild from the history
  // database.
  void OnCacheRestoreDone(bool succeeded);

  // Notifies all observers that the index has been loaded or rebuilt.
  void NotifyHasLoaded();

  // Rebuilds the index from the history database if the history database has
  // been loaded, otherwise registers for the history loaded notification so
  // that the rebuild can take place at a later time.
  void RebuildFromHistoryIfLoaded();

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

  // Posts a task to completely reset the private data and the backing cache.
  void PostResetPrivateDataTask();

  // Posts a task to completely replace the cache database with a current
  // image of the index private data.
  void PostRefreshCacheTask();

  // Callback used by PostRefreshCacheTask and PostResetPrivateDataTask to
  // notify observers that the cache database contents have been refreshed or
  // reset and that the loading of the index is complete.
  void OnCacheRefreshOrResetDone();

  // Attempts to refresh the cache database in response to a notification that
  // an update transaction has failed. If the refresh fails then the cache
  // database is ignored and an attempt will be made to rebuild the cache
  // the next time the associated profile is opened.
  void RepairCacheDatabase();

  // Returns a pointer to our private data.
  scoped_refptr<URLIndexPrivateData> private_data() { return private_data_; }

  // Returns the blocking pool sequence token.
  base::SequencedWorkerPool::SequenceToken sequence_token_for_testing() {
    return sequence_token_;
  }

  // The profile, may be null when testing.
  Profile* profile_;

  // Languages used during the word-breaking process during indexing.
  std::string languages_;

  // Directory where cache database or protobuf-based cache file resides.
  // This is, except when unit testing, the same directory in which the
  // profile's history database is found.
  FilePath history_dir_;

  // The index's durable private data.
  scoped_refptr<URLIndexPrivateData> private_data_;

  // Sequence token for coordinating database tasks. This is shared with
  // our private data and its cache database.
  const base::SequencedWorkerPool::SequenceToken sequence_token_;

  bool index_available_;  // True when index is available for updating.

  // Contains index updates queued up while the index is unavailable. This
  // usually during profile startup.
  enum UpdateType { UPDATE_VISIT, DELETE_VISIT };

  struct IndexUpdateItem {
    IndexUpdateItem(UpdateType update_type, URLRow row);
    ~IndexUpdateItem();

    UpdateType update_type;
    URLRow row;   // The row to be updated or deleted.
  };
  typedef std::vector<IndexUpdateItem> PendingUpdates;
  PendingUpdates pending_updates_;

  ObserverList<InMemoryURLIndex::Observer> observers_;

  CancelableRequestConsumer cache_reader_consumer_;
  content::NotificationRegistrar registrar_;

  // Set to true once the shutdown process has begun.
  bool shutdown_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<InMemoryURLIndex> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLIndex);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
