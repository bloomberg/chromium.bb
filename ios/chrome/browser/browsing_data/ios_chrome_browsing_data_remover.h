// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/prefs/pref_member.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ios {
class ChromeBrowserState;
}

namespace net {
class URLRequestContextGetter;
}

// IOSChromeBrowsingDataRemover is responsible for removing data related to
// browsing: visits in url database, downloads, cookies ...

class IOSChromeBrowsingDataRemover {
 public:
  // Time period ranges available when doing browsing data removals.
  enum TimePeriod { EVERYTHING };

  // Mask used for Remove.
  enum RemoveDataMask {
    REMOVE_APPCACHE = 1 << 0,
    REMOVE_CACHE = 1 << 1,
    REMOVE_COOKIES = 1 << 2,
    REMOVE_DOWNLOADS = 1 << 3,
    REMOVE_FORM_DATA = 1 << 4,
    // In addition to visits, REMOVE_HISTORY removes keywords and last session.
    REMOVE_HISTORY = 1 << 5,
    REMOVE_INDEXEDDB = 1 << 6,
    REMOVE_LOCAL_STORAGE = 1 << 7,
    REMOVE_PASSWORDS = 1 << 8,
    REMOVE_WEBSQL = 1 << 9,
    REMOVE_CHANNEL_IDS = 1 << 10,
    REMOVE_GOOGLE_APP_LAUNCHER_DATA = 1 << 11,
    REMOVE_CACHE_STORAGE = 1 << 12,
    REMOVE_VISITED_LINKS = 1 << 13,

    // "Site data" includes cookies, appcache, file systems, indexedDBs, local
    // storage, webSQL, service workers, cache storage, plugin data, and web app
    // data (on Android).
    REMOVE_SITE_DATA = REMOVE_APPCACHE | REMOVE_COOKIES | REMOVE_INDEXEDDB |
                       REMOVE_LOCAL_STORAGE |
                       REMOVE_CACHE_STORAGE |
                       REMOVE_WEBSQL |
                       REMOVE_CHANNEL_IDS |
                       REMOVE_VISITED_LINKS,

    // Includes all the available remove options. Meant to be used by clients
    // that wish to wipe as much data as possible from a ChromeBrowserState, to
    // make it look like a new ChromeBrowserState.
    REMOVE_ALL = REMOVE_SITE_DATA | REMOVE_CACHE | REMOVE_DOWNLOADS |
                 REMOVE_FORM_DATA |
                 REMOVE_HISTORY |
                 REMOVE_PASSWORDS,
  };

  // A helper enum to report the deletion of cookies and/or cache. Do not
  // reorder the entries, as this enum is passed to UMA.
  enum CookieOrCacheDeletionChoice {
    NEITHER_COOKIES_NOR_CACHE,
    ONLY_COOKIES,
    ONLY_CACHE,
    BOTH_COOKIES_AND_CACHE,
    MAX_CHOICE_VALUE
  };

  // When IOSChromeBrowsingDataRemover successfully removes data, a
  // notification of type NOTIFICATION_BROWSING_DATA_REMOVED is triggered with
  // a Details object of this type.
  struct NotificationDetails {
    NotificationDetails();
    NotificationDetails(const NotificationDetails& details);
    NotificationDetails(base::Time removal_begin, int removal_mask);
    ~NotificationDetails();

    // The beginning of the removal time range.
    base::Time removal_begin;

    // The removal mask (see the RemoveDataMask enum for details).
    int removal_mask;
  };

  // Observer is notified when the removal is done. Done means keywords have
  // been deleted, cache cleared and all other tasks scheduled.
  class Observer {
   public:
    virtual void OnIOSChromeBrowsingDataRemoverDone() = 0;

   protected:
    virtual ~Observer() {}
  };

  using Callback = base::Callback<void(const NotificationDetails&)>;
  using CallbackSubscription = scoped_ptr<
      base::CallbackList<void(const NotificationDetails&)>::Subscription>;

  // Creates a IOSChromeBrowsingDataRemover bound to a specific period of time
  // (as defined via a TimePeriod). Returns a raw pointer, as
  // IOSChromeBrowsingDataRemover retains ownership of itself, and deletes
  // itself once finished.
  static IOSChromeBrowsingDataRemover* CreateForPeriod(
      ios::ChromeBrowserState* browser_state,
      TimePeriod period);

  // Calculate the begin time for the deletion range specified by |time_period|.
  static base::Time CalculateBeginDeleteTime(TimePeriod time_period);

  // Is the IOSChromeBrowsingDataRemover currently in the process of removing
  // data?
  static bool is_removing() { return is_removing_; }

  // Add a callback to the list of callbacks to be called during a browsing data
  // removal event. Returns a subscription object that can be used to
  // un-register the callback.
  static CallbackSubscription RegisterOnBrowsingDataRemovedCallback(
      const Callback& callback);

  // Removes the specified items related to browsing for all origins.
  void Remove(int remove_mask);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when history deletion is done.
  void OnHistoryDeletionDone();

 private:
  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  static void set_removing(bool is_removing);

  // Creates a IOSChromeBrowsingDataRemover to remove browser data from the
  // specified profile in the specified time range. Use Remove to initiate the
  // removal.
  IOSChromeBrowsingDataRemover(ios::ChromeBrowserState* browser_state,
                               base::Time delete_begin,
                               base::Time delete_end);

  // IOSChromeBrowsingDataRemover deletes itself (using DeleteHelper) and is
  // not supposed to be deleted by other objects so make destructor private and
  // DeleteHelper a friend.
  friend class base::DeleteHelper<IOSChromeBrowsingDataRemover>;

  ~IOSChromeBrowsingDataRemover();

  // Callback for when TemplateURLService has finished loading. Clears the data,
  // clears the respective waiting flag, and invokes NotifyAndDeleteIfDone.
  void OnKeywordsLoaded();

  // Removes the specified items related to browsing for a specific host. If the
  // provided |remove_url| is empty, data is removed for all origins.
  // TODO(mkwst): The current implementation relies on unique (empty) origins to
  // signal removal of all origins. Reconsider this behavior if/when we build
  // a "forget this site" feature.
  void RemoveImpl(int remove_mask, const GURL& remove_url);

  // Notifies observers and deletes this object.
  void NotifyAndDelete();

  // Checks if we are all done, and if so, calls NotifyAndDelete().
  void NotifyAndDeleteIfDone();

  // Callback for when the hostname resolution cache has been cleared.
  // Clears the respective waiting flag and invokes NotifyAndDeleteIfDone.
  void OnClearedHostnameResolutionCache();

  // Callback for when network related data in ProfileIOData has been cleared.
  // Clears the respective waiting flag and invokes NotifyAndDeleteIfDone.
  void OnClearedNetworkingHistory();

  // Callback for when the cache has been deleted. Invokes
  // NotifyAndDeleteIfDone.
  void OnClearedCache(int error);

  // Callback for when passwords for the requested time range have been cleared.
  void OnClearedPasswords();

  // Callback for when Cookies has been deleted. Invokes NotifyAndDeleteIfDone.
  void OnClearedCookies(int num_deleted);

  // Invoked on the IO thread to delete cookies.
  void ClearCookiesOnIOThread(
      const scoped_refptr<net::URLRequestContextGetter>& rq_context,
      const GURL& storage_url);

  // Invoked on the IO thread to delete channel IDs.
  void ClearChannelIDsOnIOThread(
      const scoped_refptr<net::URLRequestContextGetter>& rq_context);

  // Callback on IO Thread when channel IDs have been deleted. Clears SSL
  // connection pool and posts to UI thread to run OnClearedChannelIDs.
  void OnClearedChannelIDsOnIOThread(
      const scoped_refptr<net::URLRequestContextGetter>& rq_context);

  // Callback for when channel IDs have been deleted. Invokes
  // NotifyAndDeleteIfDone.
  void OnClearedChannelIDs();

  // Callback from the above method.
  void OnClearedFormData();

  // Callback for when the Autofill profile and credit card origin URLs have
  // been deleted.
  void OnClearedAutofillOriginURLs();

  void OnClearedDomainReliabilityMonitor();

  // Returns true if we're all done.
  bool AllDone();

  // ChromeBrowserState we're to remove from.
  ios::ChromeBrowserState* browser_state_;

  // Start time to delete from.
  const base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // True if Remove has been invoked.
  static bool is_removing_;

  // Used to delete data from HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> main_context_getter_;

  // True if we're waiting for various data to be deleted.
  // These may only be accessed from UI thread in order to avoid races!
  bool waiting_for_clear_autofill_origin_urls_ = false;
  bool waiting_for_clear_cache_ = false;
  bool waiting_for_clear_channel_ids_ = false;
  // Non-zero if waiting for cookies to be cleared.
  int waiting_for_clear_cookies_count_ = 0;
  bool waiting_for_clear_form_ = false;
  bool waiting_for_clear_history_ = false;
  bool waiting_for_clear_hostname_resolution_cache_ = false;
  bool waiting_for_clear_keyword_data_ = false;
  bool waiting_for_clear_networking_history_ = false;
  bool waiting_for_clear_passwords_ = false;

  // The removal mask for the current removal operation.
  int remove_mask_ = 0;

  base::ObserverList<Observer> observer_list_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  scoped_ptr<TemplateURLService::Subscription> template_url_sub_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeBrowsingDataRemover);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_
