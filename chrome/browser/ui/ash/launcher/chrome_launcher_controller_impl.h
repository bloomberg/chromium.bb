// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_IMPL_H_

#include <list>
#include <memory>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_model_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/ash/app_sync_ui_state_observer.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_updater.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "ui/aura/window_observer.h"

class AppSyncUIState;
class BrowserStatusMonitor;
class Profile;
class AppWindowLauncherController;

namespace ash {
class ShelfModel;
namespace launcher {
class ChromeLauncherPrefsObserver;
}
}

class ChromeLauncherControllerUserSwitchObserver;

// Implementation of ChromeLauncherController, used for classic Ash.
// This class manipulates Ash's ShelfModel to support Chrome and its apps.
class ChromeLauncherControllerImpl
    : public ChromeLauncherController,
      public LauncherAppUpdater::Delegate,
      private ash::ShelfModelObserver,
      private ash::WindowTreeHostManager::Observer,
      private AppSyncUIStateObserver,
      private app_list::AppListSyncableService::Observer,
      private sync_preferences::PrefServiceSyncableObserver {
 public:
  ChromeLauncherControllerImpl(Profile* profile, ash::ShelfModel* model);
  ~ChromeLauncherControllerImpl() override;

  // ChromeLauncherController:
  ash::ShelfID CreateAppLauncherItem(
      std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
      ash::ShelfItemStatus status) override;
  const ash::ShelfItem* GetItem(ash::ShelfID id) const override;
  void SetItemType(ash::ShelfID id, ash::ShelfItemType type) override;
  void SetItemStatus(ash::ShelfID id, ash::ShelfItemStatus status) override;
  void CloseLauncherItem(ash::ShelfID id) override;
  bool IsPinned(ash::ShelfID id) override;
  void SetV1AppStatus(const std::string& app_id,
                      ash::ShelfItemStatus status) override;
  void Launch(ash::ShelfID id, int event_flags) override;
  void Close(ash::ShelfID id) override;
  bool IsOpen(ash::ShelfID id) override;
  bool IsPlatformApp(ash::ShelfID id) override;
  void ActivateApp(const std::string& app_id,
                   ash::ShelfLaunchSource source,
                   int event_flags) override;
  void SetLauncherItemImage(ash::ShelfID shelf_id,
                            const gfx::ImageSkia& image) override;
  void UpdateAppState(content::WebContents* contents,
                      AppState app_state) override;
  ash::ShelfID GetShelfIDForWebContents(
      content::WebContents* contents) override;
  void SetRefocusURLPatternForTest(ash::ShelfID id, const GURL& url) override;
  ash::ShelfAction ActivateWindowOrMinimizeIfActive(
      ui::BaseWindow* window,
      bool allow_minimize) override;
  void ActiveUserChanged(const std::string& user_email) override;
  void AdditionalUserAddedToSession(Profile* profile) override;
  ash::MenuItemList GetAppMenuItemsForTesting(
      const ash::ShelfItem& item) override;
  std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      const std::string& app_id) override;
  void ActivateShellApp(const std::string& app_id, int window_index) override;
  bool IsWebContentHandledByApplication(content::WebContents* web_contents,
                                        const std::string& app_id) override;
  bool ContentCanBeHandledByGmailApp(
      content::WebContents* web_contents) override;
  gfx::Image GetAppListIcon(content::WebContents* web_contents) const override;
  base::string16 GetAppListTitle(
      content::WebContents* web_contents) const override;
  BrowserShortcutLauncherItemController*
  GetBrowserShortcutLauncherItemController() override;
  bool ShelfBoundsChangesProbablyWithUser(
      ash::WmShelf* shelf,
      const AccountId& account_id) const override;
  void OnUserProfileReadyToSwitch(Profile* profile) override;
  ArcAppDeferredLauncherController* GetArcDeferredLauncher() override;
  const std::string& GetLaunchIDForShelfID(ash::ShelfID id) override;
  void AttachProfile(Profile* profile_to_attach) override;

  ash::ShelfModel* shelf_model() const { return model_; }

  // Access to the BrowserStatusMonitor for tests.
  BrowserStatusMonitor* browser_status_monitor_for_test() {
    return browser_status_monitor_.get();
  }

  // Access to the AppWindowLauncherController list for tests.
  const std::vector<std::unique_ptr<AppWindowLauncherController>>&
  app_window_controllers_for_test() {
    return app_window_controllers_;
  }

  // Helpers that call through to corresponding ShelfModel functions.
  ash::ShelfID GetShelfIDForAppID(const std::string& app_id);
  ash::ShelfID GetShelfIDForAppIDAndLaunchID(const std::string& app_id,
                                             const std::string& launch_id);
  const std::string& GetAppIDForShelfID(ash::ShelfID id);
  void PinAppWithID(const std::string& app_id);
  bool IsAppPinned(const std::string& app_id);
  void UnpinAppWithID(const std::string& app_id);

  // LauncherAppUpdater::Delegate:
  void OnAppInstalled(content::BrowserContext* browser_context,
                      const std::string& app_id) override;
  void OnAppUpdated(content::BrowserContext* browser_context,
                    const std::string& app_id) override;
  void OnAppUninstalledPrepared(content::BrowserContext* browser_context,
                                const std::string& app_id) override;

 protected:
  // ChromeLauncherController:
  void OnInit() override;

  // Creates a new app shortcut item and controller on the shelf at |index|.
  ash::ShelfID CreateAppShortcutLauncherItem(
      const ash::AppLaunchId& app_launch_id,
      int index);

 private:
  friend class ChromeLauncherControllerImplTest;
  friend class ShelfAppBrowserTest;
  friend class LauncherPlatformAppBrowserTest;
  friend class TestChromeLauncherControllerImpl;
  FRIEND_TEST_ALL_PREFIXES(ChromeLauncherControllerImplTest, AppPanels);

  typedef std::map<content::WebContents*, std::string> WebContentsToAppIDMap;

  // Remembers / restores list of running applications.
  // Note that this order will neither be stored in the preference nor will it
  // remember the order of closed applications since it is only temporary.
  void RememberUnpinnedRunningApplicationOrder();
  void RestoreUnpinnedRunningApplicationOrder(const std::string& user_id);

  // Invoked when the associated browser or app is closed.
  void RemoveShelfItem(ash::ShelfID id);

  // Internal helpers for pinning and unpinning that handle both
  // client-triggered and internal pinning operations.
  void DoPinAppWithID(const std::string& app_id);
  void DoUnpinAppWithID(const std::string& app_id, bool update_prefs);

  // Pin a running app with |shelf_id| internally to |index|.
  void PinRunningAppInternal(int index, ash::ShelfID shelf_id);

  // Unpin a locked application. This is an internal call which converts the
  // model type of the given app index from a shortcut into an unpinned running
  // app.
  void UnpinRunningAppInternal(int index);

  // Updates pin position for the item specified by |id| in sync model.
  void SyncPinPosition(ash::ShelfID id);

  // Re-syncs shelf model.
  void UpdateAppLaunchersFromPref();

  // Schedules re-sync of shelf model.
  void ScheduleUpdateAppLaunchersFromPref();

  // Update the policy-pinned flag for each shelf item.
  void UpdatePolicyPinnedAppsFromPrefs();

  // Sets whether the virtual keyboard is enabled from prefs.
  void SetVirtualKeyboardBehaviorFromPrefs();

  // Returns the shelf item status for the given |app_id|, which can be either
  // STATUS_ACTIVE (if the app is active), STATUS_RUNNING (if there is such an
  // app) or STATUS_CLOSED.
  ash::ShelfItemStatus GetAppState(const std::string& app_id);

  // Creates an app launcher to insert at |index|. Note that |index| may be
  // adjusted by the model to meet ordering constraints.
  // The |shelf_item_type| will be set into the ShelfModel.
  ash::ShelfID InsertAppLauncherItem(
      std::unique_ptr<ash::ShelfItemDelegate> item_delegate,
      ash::ShelfItemStatus status,
      int index,
      ash::ShelfItemType shelf_item_type);

  // Create ShelfItem for Browser Shortcut.
  void CreateBrowserShortcutLauncherItem();

  // Check if the given |web_contents| is in incognito mode.
  bool IsIncognito(const content::WebContents* web_contents) const;

  // Finds the index of where to insert the next item.
  int FindInsertionPoint();

  // Close all windowed V1 applications of a certain extension which was already
  // deleted.
  void CloseWindowedAppsFromRemovedExtension(const std::string& app_id,
                                             const Profile* profile);

  // Forget the current profile to allow attaching to a new one.
  void ReleaseProfile();

  // ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override;
  void ShelfItemRemoved(int index, const ash::ShelfItem& old_item) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override;

  // ash::WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // AppSyncUIStateObserver:
  void OnAppSyncUIStatusChanged() override;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override;

  // sync_preferences::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  // An internal helper to unpin a shelf item; this does not update prefs.
  void UnpinShelfItemInternal(ash::ShelfID id);

  ash::ShelfModel* model_;

  // Direct access to app_id for a web contents.
  WebContentsToAppIDMap web_contents_to_app_id_;

  // Used to track app windows.
  std::vector<std::unique_ptr<AppWindowLauncherController>>
      app_window_controllers_;

  // Used to handle app load/unload events.
  std::vector<std::unique_ptr<LauncherAppUpdater>> app_updaters_;

  PrefChangeRegistrar pref_change_registrar_;

  AppSyncUIState* app_sync_ui_state_ = nullptr;

  // The owned browser status monitor.
  std::unique_ptr<BrowserStatusMonitor> browser_status_monitor_;

  // A special observer class to detect user switches.
  std::unique_ptr<ChromeLauncherControllerUserSwitchObserver>
      user_switch_observer_;

  std::unique_ptr<ash::launcher::ChromeLauncherPrefsObserver> prefs_observer_;

  std::unique_ptr<ArcAppDeferredLauncherController> arc_deferred_launcher_;

  // The list of running & un-pinned applications for different users on hidden
  // desktops.
  typedef std::vector<std::string> RunningAppListIds;
  typedef std::map<std::string, RunningAppListIds> RunningAppListIdMap;
  RunningAppListIdMap last_used_running_application_order_;

  base::WeakPtrFactory<ChromeLauncherControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_IMPL_H_
