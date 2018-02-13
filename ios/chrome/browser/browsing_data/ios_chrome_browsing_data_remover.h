// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/prefs/pref_member.h"
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
    REMOVE_CACHE_STORAGE = 1 << 11,
    REMOVE_VISITED_LINKS = 1 << 12,

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
  using CallbackSubscription = std::unique_ptr<
      base::CallbackList<void(const NotificationDetails&)>::Subscription>;

  // Creates a IOSChromeBrowsingDataRemover bound to a specific period of time
  // (as defined via a TimePeriod). Returns a raw pointer, as
  // IOSChromeBrowsingDataRemover retains ownership of itself, and deletes
  // itself once finished.
  static IOSChromeBrowsingDataRemover* CreateForPeriod(
      ios::ChromeBrowserState* browser_state,
      browsing_data::TimePeriod period);

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
  // and invoke the callback.
  void OnKeywordsLoaded(base::OnceClosure callback);

  // Removes the specified items related to browsing for a specific host. If the
  // provided |remove_url| is empty, data is removed for all origins.
  // TODO(mkwst): The current implementation relies on unique (empty) origins to
  // signal removal of all origins. Reconsider this behavior if/when we build
  // a "forget this site" feature.
  void RemoveImpl(int remove_mask);

  // Notifies observers and deletes this object.
  void NotifyAndDelete();

  // Called by the closures returned by CreatePendingTaskCompletionClosure().
  // Checks if all tasks have completed, and if so, calls Notify().
  void OnTaskComplete();

  // Increments the number of pending tasks by one, and returns a OnceClosure
  // that calls OnTaskComplete(). The Remover is complete once all the closures
  // created by this method have been invoked.
  base::OnceClosure CreatePendingTaskCompletionClosure();

  // Returns a weak pointer to IOSChromeBrowsingDataRemover for internal
  // purposes.
  base::WeakPtr<IOSChromeBrowsingDataRemover> GetWeakPtr();

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

  // Number of pending tasks.
  int pending_tasks_count_ = 0;

  // The removal mask for the current removal operation.
  int remove_mask_ = 0;

  base::ObserverList<Observer> observer_list_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  base::WeakPtrFactory<IOSChromeBrowsingDataRemover> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeBrowsingDataRemover);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_IOS_CHROME_BROWSING_DATA_REMOVER_H_
