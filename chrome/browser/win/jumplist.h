// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_H_
#define CHROME_BROWSER_WIN_JUMPLIST_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/win/jumplist_updater.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

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
class JumpList : public sessions::TabRestoreServiceObserver,
                 public history::TopSitesObserver,
                 public KeyedService {
 public:
  // KeyedService:
  void Shutdown() override;

  // Returns true if the custom JumpList is enabled.
  static bool Enabled();

 private:
  using UrlAndLinkItem = std::pair<std::string, scoped_refptr<ShellLinkItem>>;
  using URLIconCache = base::flat_map<std::string, base::FilePath>;

  // Holds results of the RunUpdateJumpList run.
  struct UpdateResults {
    UpdateResults();
    ~UpdateResults();

    // Icon file paths of the most visited links, indexed by tab url.
    // Holding a copy of most_visited_icons_ initially, it's updated by the
    // JumpList update run. If the update run succeeds, it overwrites
    // most_visited_icons_.
    URLIconCache most_visited_icons_in_update;

    // icon file paths of the recently closed links, indexed by tab url.
    // Holding a copy of recently_closed_icons_ initially, it's updated by the
    // JumpList update run. If the update run succeeds, it overwrites
    // recently_closed_icons_.
    URLIconCache recently_closed_icons_in_update;

    // A flag indicating if a JumpList update run is successful.
    bool update_success = false;

    // A flag indicating if there is a timeout in notifying the JumpList update
    // to shell. Note that this variable is independent of update_success.
    bool update_timeout = false;
  };

  friend JumpListFactory;
  explicit JumpList(Profile* profile);  // Use JumpListFactory instead

  ~JumpList() override;

  // Adds a new ShellLinkItem for |tab| to the JumpList data provided that doing
  // so will not exceed |max_items|.
  bool AddTab(const sessions::TabRestoreService::Tab& tab, size_t max_items);

  // Adds a new ShellLinkItem for each tab in |window| to the JumpList data
  // provided that doing so will not exceed |max_items|.
  void AddWindow(const sessions::TabRestoreService::Window& window,
                 size_t max_items);

  // Starts loading a favicon for each URL in |icon_urls_|.
  // This function sends a query to HistoryService.
  // When finishing loading all favicons, this function posts a task that
  // decompresses collected favicons and updates a JumpList.
  void StartLoadingFavicon();

  // Callback for HistoryService that notifies when a requested favicon is
  // available. To avoid file operations, this function just attaches the given
  // |image_result| to a ShellLinkItem object.
  void OnFaviconDataAvailable(
      const favicon_base::FaviconImageResult& image_result);

  // Callback for TopSites that notifies when |data|, the "Most Visited" list,
  // is available. This function updates the ShellLinkItemList objects and
  // begins the process of fetching favicons for the URLs.
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& data);

  // Callback for changes to the incognito mode availability pref.
  void OnIncognitoAvailabilityChanged();

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  // history::TopSitesObserver:
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  // Initializes the one-shot timer to update the JumpList in a while. If there
  // is already a request queued then cancel it and post the new request. This
  // ensures that JumpList update won't happen until there has been a brief
  // quiet period, thus avoiding update storms.
  void InitializeTimerForUpdate();

  // Called on a timer after requests storms have subsided. Calls APIs
  // ProcessTopSitesNotification and ProcessTabRestoreNotification on
  // demand to do the actual work.
  void OnDelayTimer();

  // Processes notifications from TopSites service.
  void ProcessTopSitesNotification();

  // Processes notifications from TabRestore service.
  void ProcessTabRestoreServiceNotification();

  // Posts tasks to update the JumpList and delete any obsolete JumpList related
  // folders.
  void PostRunUpdate();

  // Deletes icon files in |icon_dir| which are not in |icon_cache| anymore.
  static void DeleteIconFiles(const base::FilePath& icon_dir,
                              URLIconCache* icon_cache);

  // In |icon_dir|, creates at most |max_items| icon files which are not in
  // |icon_cache| for the asynchrounously loaded icons stored in |item_list|.
  // |icon_cache| is also updated for newly created icons.
  // Returns the number of new icon files created.
  static int CreateIconFiles(const base::FilePath& icon_dir,
                             const ShellLinkItemList& item_list,
                             size_t max_items,
                             URLIconCache* icon_cache);

  // Updates icon files for |page_list| in |icon_dir|, which consists of
  // 1) creating at most |slot_limit| new icons which are not in |icon_cache|;
  // 2) deleting old icons which are not in |icon_cache|.
  // Returns the number of new icon files created.
  static int UpdateIconFiles(const base::FilePath& icon_dir,
                             const ShellLinkItemList& page_list,
                             size_t slot_limit,
                             URLIconCache* icon_cache);

  // Updates the application JumpList, which consists of 1) create new icon
  // files; 2) delete obsolete icon files; 3) notify the OS.
  // Note that any timeout error along the way results in the old JumpList being
  // left as-is, while any non-timeout error results in the old JumpList being
  // left as-is, but without icon files.
  static void RunUpdateJumpList(
      const base::string16& app_id,
      const base::FilePath& profile_dir,
      const ShellLinkItemList& most_visited_pages,
      const ShellLinkItemList& recently_closed_pages,
      bool most_visited_pages_have_updates,
      bool recently_closed_pages_have_updates,
      IncognitoModePrefs::Availability incognito_availability,
      UpdateResults* update_results);

  // Callback for RunUpdateJumpList that notifies when it finishes running.
  // Updates certain JumpList member variables and/or triggers a new JumpList
  // update based on |update_results|.
  void OnRunUpdateCompletion(std::unique_ptr<UpdateResults> update_results);

  // Cancels a pending JumpList update.
  void CancelPendingUpdate();

  // Terminates the JumpList, which includes cancelling any pending updates and
  // stopping observing the Profile and its services. This must be called before
  // the |profile_| is destroyed.
  void Terminate();

  // Tracks FaviconService tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The Profile object is used to listen for events.
  Profile* profile_;

  // Manages the registration of pref change observers.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // App id to associate with the JumpList.
  base::string16 app_id_;

  // Timer for requesting delayed JumpList updates.
  base::OneShotTimer timer_;

  // A list of URLs we need to retrieve their favicons,
  std::list<UrlAndLinkItem> icon_urls_;

  // Items in the "Most Visited" category of the JumpList.
  ShellLinkItemList most_visited_pages_;

  // Items in the "Recently Closed" category of the JumpList.
  ShellLinkItemList recently_closed_pages_;

  // The icon file paths of the most visited links, indexed by tab url.
  URLIconCache most_visited_icons_;

  // The icon file paths of the recently closed links, indexed by tab url.
  URLIconCache recently_closed_icons_;

  // A flag indicating if TopSites service has notifications.
  bool top_sites_has_pending_notification_ = false;

  // A flag indicating if TabRestore service has notifications.
  bool tab_restore_has_pending_notification_ = false;

  // A flag indicating if "Most Visited" category should be updated.
  bool most_visited_should_update_ = false;

  // A flag indicating if "Recently Closed" category should be updated.
  bool recently_closed_should_update_ = false;

  // A flag indicating if there's a JumpList update task already posted or
  // currently running.
  bool update_in_progress_ = false;

  // A flag indicating if a session has at least one tab closed.
  bool has_tab_closed_ = false;

  // Number of updates to skip to alleviate the machine when a previous update
  // was too slow. Updates will be resumed when this reaches 0 again.
  int updates_to_skip_ = 0;

  // Id of last favicon task. It's used to cancel current task if a new one
  // comes in before it finishes.
  base::CancelableTaskTracker::TaskId task_id_ =
      base::CancelableTaskTracker::kBadTaskId;

  // A task runner running tasks to update the JumpList.
  scoped_refptr<base::SingleThreadTaskRunner> update_jumplist_task_runner_;

  // A task runner running tasks to delete the JumpListIcons and
  // JumpListIconsOld folders.
  scoped_refptr<base::SequencedTaskRunner> delete_jumplisticons_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // For callbacks may run after destruction.
  base::WeakPtrFactory<JumpList> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(JumpList);
};

#endif  // CHROME_BROWSER_WIN_JUMPLIST_H_
