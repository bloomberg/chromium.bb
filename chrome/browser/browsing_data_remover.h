// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_REMOVER_H_
#pragma once

#include <set>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/time.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/cancelable_request.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/quota/quota_types.h"

class ExtensionSpecialStoragePolicy;
class IOThread;
class PluginDataRemover;
class Profile;

namespace disk_cache {
class Backend;
}

namespace net {
class URLRequestContextGetter;
}

namespace quota {
class QuotaManager;
}

// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...

class BrowsingDataRemover : public content::NotificationObserver,
                            public base::WaitableEventWatcher::Delegate {
 public:
  // Time period ranges available when doing browsing data removals.
  enum TimePeriod {
    LAST_HOUR = 0,
    LAST_DAY,
    LAST_WEEK,
    FOUR_WEEKS,
    EVERYTHING
  };

  // Mask used for Remove.
  enum RemoveDataMask {
    REMOVE_APPCACHE = 1 << 0,
    REMOVE_CACHE = 1 << 1,
    REMOVE_COOKIES = 1 << 2,
    REMOVE_DOWNLOADS = 1 << 3,
    REMOVE_FILE_SYSTEMS = 1 << 4,
    REMOVE_FORM_DATA = 1 << 5,
    // In addition to visits, REMOVE_HISTORY removes keywords and last session.
    REMOVE_HISTORY = 1 << 6,
    REMOVE_INDEXEDDB = 1 << 7,
    REMOVE_LOCAL_STORAGE = 1 << 8,
    REMOVE_LSO_DATA = 1 << 9,
    REMOVE_PASSWORDS = 1 << 10,
    REMOVE_WEBSQL = 1 << 11,

    // "Site data" includes cookies, appcache, file systems, indexedDBs, local
    // storage, webSQL, and LSO data.
    REMOVE_SITE_DATA = REMOVE_APPCACHE | REMOVE_COOKIES | REMOVE_FILE_SYSTEMS |
                       REMOVE_INDEXEDDB | REMOVE_LOCAL_STORAGE |
                       REMOVE_LSO_DATA | REMOVE_WEBSQL
  };

  // Observer is notified when the removal is done. Done means keywords have
  // been deleted, cache cleared and all other tasks scheduled.
  class Observer {
   public:
    virtual void OnBrowsingDataRemoverDone() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Creates a BrowsingDataRemover to remove browser data from the specified
  // profile in the specified time range. Use Remove to initiate the removal.
  BrowsingDataRemover(Profile* profile, base::Time delete_begin,
                      base::Time delete_end);

  // Creates a BrowsingDataRemover to remove browser data from the specified
  // profile in the specified time range.
  BrowsingDataRemover(Profile* profile, TimePeriod time_period,
                      base::Time delete_end);

  // Removes the specified items related to browsing.
  void Remove(int remove_mask);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when history deletion is done.
  void OnHistoryDeletionDone();

  static bool is_removing() { return removing_; }

 private:
  // The clear API needs to be able to toggle removing_ in order to test that
  // only one BrowsingDataRemover instance can be called at a time.
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest, ClearOneAtATime);

  enum CacheState {
    STATE_NONE,
    STATE_CREATE_MAIN,
    STATE_CREATE_MEDIA,
    STATE_DELETE_MAIN,
    STATE_DELETE_MEDIA,
    STATE_DONE
  };

  // BrowsingDataRemover deletes itself (using DeleteTask) and is not supposed
  // to be deleted by other objects so make destructor private and DeleteTask
  // a friend.
  friend class DeleteTask<BrowsingDataRemover>;
  virtual ~BrowsingDataRemover();

  // content::NotificationObserver method. Callback when TemplateURLService has
  // finished loading. Deletes the entries from the model, and if we're not
  // waiting on anything else notifies observers and deletes this
  // BrowsingDataRemover.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // WaitableEventWatcher implementation.
  // Called when plug-in data has been cleared. Invokes NotifyAndDeleteIfDone.
  virtual void OnWaitableEventSignaled(base::WaitableEvent* waitable_event);

  // If we're not waiting on anything, notifies observers and deletes this
  // object.
  void NotifyAndDeleteIfDone();

  // Callback when the network history has been deleted. Invokes
  // NotifyAndDeleteIfDone.
  void ClearedNetworkHistory();

  // Invoked on the IO thread to clear the HostCache, speculative data about
  // subresources on visited sites, and initial navigation history.
  void ClearNetworkingHistory(IOThread* io_thread);

  // Callback when the cache has been deleted. Invokes NotifyAndDeleteIfDone.
  void ClearedCache();

  // Invoked on the IO thread to delete from the cache.
  void ClearCacheOnIOThread();

  // Performs the actual work to delete the cache.
  void DoClearCache(int rv);

  // Invoked on the IO thread to delete all storage types managed by the quota
  // system: AppCache, Databases, FileSystems.
  void ClearQuotaManagedDataOnIOThread();

  // Callback to respond to QuotaManager::GetOriginsModifiedSince, which is the
  // core of 'ClearQuotaManagedDataOnIOThread'.
  void OnGotQuotaManagedOrigins(const std::set<GURL>& origins,
                                quota::StorageType type);

  // Callback responding to deletion of a single quota managed origin's
  // persistent data
  void OnQuotaManagedOriginDeletion(quota::QuotaStatusCode);

  // Called to check whether all temporary and persistent origin data that
  // should be deleted has been deleted. If everything's good to go, invokes
  // OnQuotaManagedDataDeleted on the UI thread.
  void CheckQuotaManagedDataDeletionStatus();

  // Completion handler that runs on the UI thread once persistent data has been
  // deleted. Updates the waiting flag and invokes NotifyAndDeleteIfDone.
  void OnQuotaManagedDataDeleted();

  // Callback when Cookies has been deleted. Invokes NotifyAndDeleteIfDone.
  void OnClearedCookies(int num_deleted);

  // Invoked on the IO thread to delete cookies.
  void ClearCookiesOnIOThread(net::URLRequestContextGetter* rq_context);

  // Calculate the begin time for the deletion range specified by |time_period|.
  base::Time CalculateBeginDeleteTime(TimePeriod time_period);

  // Returns true if we're all done.
  bool all_done() {
    return registrar_.IsEmpty() && !waiting_for_clear_cache_ &&
           !waiting_for_clear_cookies_&&
           !waiting_for_clear_history_ &&
           !waiting_for_clear_quota_managed_data_ &&
           !waiting_for_clear_networking_history_ &&
           !waiting_for_clear_lso_data_;
  }

  // Setter for removing_; DCHECKs that we can only start removing if we're not
  // already removing, and vice-versa.
  static void set_removing(bool removing);

  content::NotificationRegistrar registrar_;

  // Profile we're to remove from.
  Profile* profile_;

  // The QuotaManager is owned by the profile; we can use a raw pointer here,
  // and rely on the profile to destroy the object whenever it's reasonable.
  quota::QuotaManager* quota_manager_;

  // 'Protected' origins are not subject to data removal.
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;

  // Start time to delete from.
  const base::Time delete_begin_;

  // End time to delete to.
  const base::Time delete_end_;

  // True if Remove has been invoked.
  static bool removing_;

  // Used to delete data from the HTTP caches.
  net::OldCompletionCallbackImpl<BrowsingDataRemover> cache_callback_;
  CacheState next_cache_state_;
  disk_cache::Backend* cache_;

  // Used to delete data from HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> main_context_getter_;
  scoped_refptr<net::URLRequestContextGetter> media_context_getter_;

  // Used to delete plugin data.
  scoped_refptr<PluginDataRemover> plugin_data_remover_;
  base::WaitableEventWatcher watcher_;

  // True if we're waiting for various data to be deleted.
  // These may only be accessed from UI thread in order to avoid races!
  bool waiting_for_clear_history_;
  bool waiting_for_clear_quota_managed_data_;
  bool waiting_for_clear_networking_history_;
  bool waiting_for_clear_cookies_;
  bool waiting_for_clear_cache_;
  bool waiting_for_clear_lso_data_;

  // Tracking how many origins need to be deleted, and whether we're finished
  // gathering origins.
  int quota_managed_origins_to_delete_count_;
  int quota_managed_storage_types_to_delete_count_;

  ObserverList<Observer> observer_list_;

  // Used if we need to clear history.
  CancelableRequestConsumer request_consumer_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemover);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_REMOVER_H_
