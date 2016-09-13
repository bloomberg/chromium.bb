// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_H_
#define CHROME_BROWSER_WIN_JUMPLIST_H_

#include <stddef.h>

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chrome {
struct FaviconImageResult;
}

class JumpListFactory;
class PrefChangeRegistrar;
class Profile;

// A class which implements an application JumpList.
// This class encapsulates operations required for updating an application
// JumpList:
// * Retrieving "Most Visited" pages from HistoryService;
// * Retrieving strings from the application resource;
// * Adding COM objects to JumpList, etc.
//
// This class observes the tabs and policies of the given Profile and updates
// the JumpList whenever a change is detected.
//
// Updating a JumpList requires some file operations and it is not good to
// update it in a UI thread. To solve this problem, this class posts to a
// runnable method when it actually updates a JumpList.
//
// Note. base::CancelableTaskTracker is not thread safe, so we
// always delete JumpList on UI thread (the same thread it got constructed on).
class JumpList : public sessions::TabRestoreServiceObserver,
                 public history::TopSitesObserver,
                 public RefcountedKeyedService,
                 public base::NonThreadSafe {
 public:
  struct JumpListData {
    JumpListData();
    ~JumpListData();

    // Lock for most_visited_pages_, recently_closed_pages_, icon_urls_
    // as they may be used by up to 2 threads.
    base::Lock list_lock_;

    // A list of URLs we need to retrieve their favicons,
    // protected by the list_lock_.
    typedef std::pair<std::string, scoped_refptr<ShellLinkItem> > URLPair;
    std::list<URLPair> icon_urls_;

    // Items in the "Most Visited" category of the application JumpList,
    // protected by the list_lock_.
    ShellLinkItemList most_visited_pages_;

    // Items in the "Recently Closed" category of the application JumpList,
    // protected by the list_lock_.
    ShellLinkItemList recently_closed_pages_;
  };

  // Observer callback for TabRestoreService::Observer to notify when a tab is
  // added or removed.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  // Cancel a pending jumplist update.
  void CancelPendingUpdate();

  // Terminate the jumplist: cancel any pending updates and stop observing
  // the Profile and its services. This must be called before the |profile_|
  // is destroyed.
  void Terminate();

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // Returns true if the custom JumpList is enabled.
  // The custom jumplist works only on Windows 7 and above.
  static bool Enabled();

 private:
  friend JumpListFactory;
  explicit JumpList(Profile* profile);  // Use JumpListFactory instead
  ~JumpList() override;

  // Creates a ShellLinkItem object from a tab (or a window) and add it to the
  // given list.
  // These functions are copied from the RecentlyClosedTabsHandler class for
  // compatibility with the new-tab page.
  bool AddTab(const sessions::TabRestoreService::Tab& tab,
              ShellLinkItemList* list,
              size_t max_items);
  void AddWindow(const sessions::TabRestoreService::Window& window,
                 ShellLinkItemList* list,
                 size_t max_items);

  // Starts loading a favicon for each URL in |icon_urls_|.
  // This function sends a query to HistoryService.
  // When finishing loading all favicons, this function posts a task that
  // decompresses collected favicons and updates a JumpList.
  void StartLoadingFavicon();

  // A callback function for HistoryService that notify when a requested favicon
  // is available.
  // To avoid file operations, this function just attaches the given data to
  // a ShellLinkItem object.
  void OnFaviconDataAvailable(
      const favicon_base::FaviconImageResult& image_result);

  // Callback for TopSites that notifies when the "Most
  // Visited" list is available. This function updates the ShellLinkItemList
  // objects and send another query that retrieves a favicon for each URL in
  // the list.
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& data);

  // Callback for changes to the incognito mode availability pref.
  void OnIncognitoAvailabilityChanged();

  // Helper for RunUpdate() that determines its parameters.
  void PostRunUpdate();

  // Called on a timer to invoke RunUpdateOnFileThread() after requests storms
  // have subsided.
  void DeferredRunUpdate();

  // history::TopSitesObserver implementation.
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  // Tracks FaviconService tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The Profile object is used to listen for events
  Profile* profile_;

  // Lives on the UI thread.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // App id to associate with the jump list.
  std::wstring app_id_;

  // The directory which contains JumpList icons.
  base::FilePath icon_dir_;

  // Timer for requesting delayed updates of the jumplist.
  base::OneShotTimer timer_;

  // Holds data that can be accessed from multiple threads.
  scoped_refptr<base::RefCountedData<JumpListData>> jumplist_data_;

  // Id of last favicon task. It's used to cancel current task if a new one
  // comes in before it finishes.
  base::CancelableTaskTracker::TaskId task_id_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<JumpList> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(JumpList);
};

#endif  // CHROME_BROWSER_WIN_JUMPLIST_H_
