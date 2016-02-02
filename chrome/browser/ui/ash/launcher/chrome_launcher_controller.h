// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_model_observer.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/browser/ui/ash/app_sync_ui_state_observer.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/syncable_prefs/pref_service_syncable_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "ui/aura/window_observer.h"

class AppSyncUIState;
class Browser;
class BrowserShortcutLauncherItemController;
class BrowserStatusMonitor;
class ExtensionEnableFlow;
class GURL;
class LauncherItemController;
class Profile;
class AppWindowLauncherController;
class TabContents;

namespace ash {
class ShelfItemDelegateManager;
class ShelfModel;
}

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class BaseWindow;
}

#if defined(OS_CHROMEOS)
class ChromeLauncherControllerUserSwitchObserver;
#endif

// A list of the elements which makes up a simple menu description.
typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

// ChromeLauncherController manages the launcher items needed for content
// windows. Launcher items have a type, an optional app id, and a controller.
// This incarnation groups running tabs/windows in application specific lists.
// * Browser app windows have BrowserLauncherItemController, owned by the
//   BrowserView instance.
// * App windows have AppWindowLauncherItemController, owned by
//   AppWindowLauncherController.
// * Shortcuts have no LauncherItemController.
class ChromeLauncherController
    : public ash::ShelfDelegate,
      public ash::ShelfModelObserver,
      public ash::ShellObserver,
      public ash::WindowTreeHostManager::Observer,
      public extensions::ExtensionRegistryObserver,
      public extensions::AppIconLoader::Delegate,
      public syncable_prefs::PrefServiceSyncableObserver,
      public AppSyncUIStateObserver,
      public ExtensionEnableFlowDelegate,
      public ash::ShelfLayoutManagerObserver,
      public ash::ShelfItemDelegateManagerObserver {
 public:
  // Indicates if a shelf item is incognito or not.
  enum IncognitoState {
    STATE_INCOGNITO,
    STATE_NOT_INCOGNITO,
  };

  // Used to update the state of non plaform apps, as web contents change.
  enum AppState {
    APP_STATE_ACTIVE,
    APP_STATE_WINDOW_ACTIVE,
    APP_STATE_INACTIVE,
    APP_STATE_REMOVED
  };

  // Mockable interface to get app ids from tabs.
  class AppTabHelper {
   public:
    virtual ~AppTabHelper() {}

    // Returns the app id of the specified tab, or an empty string if there is
    // no app. All known profiles will be queried for this.
    virtual std::string GetAppID(content::WebContents* tab) = 0;

    // Returns true if |id| is valid for the currently active profile.
    // Used during restore to ignore no longer valid extensions.
    // Note that already running applications are ignored by the restore
    // process.
    virtual bool IsValidIDForCurrentUser(const std::string& id) = 0;

    // Sets the currently active profile for the usage of |GetAppID|.
    virtual void SetCurrentUser(Profile* profile) = 0;
  };

  ChromeLauncherController(Profile* profile, ash::ShelfModel* model);
  ~ChromeLauncherController() override;

  // Initializes this ChromeLauncherController.
  void Init();

  // Creates an instance.
  static ChromeLauncherController* CreateInstance(Profile* profile,
                                                  ash::ShelfModel* model);

  // Returns the single ChromeLauncherController instance.
  static ChromeLauncherController* instance() { return instance_; }

  // Creates a new app item on the shelf for |controller|.
  ash::ShelfID CreateAppLauncherItem(LauncherItemController* controller,
                                     const std::string& app_id,
                                     ash::ShelfItemStatus status);

  // Updates the running status of an item. It will also update the status of
  // browsers shelf item if needed.
  void SetItemStatus(ash::ShelfID id, ash::ShelfItemStatus status);

  // Updates the controller associated with id (which should be a shortcut).
  // |controller| will be owned by the |ChromeLauncherController| and then
  // passed on to |ShelfItemDelegateManager|.
  // TODO(skuhne): Pass in scoped_ptr to make ownership clear.
  void SetItemController(ash::ShelfID id, LauncherItemController* controller);

  // Closes or unpins the shelf item.
  void CloseLauncherItem(ash::ShelfID id);

  // Pins the specified id. Currently only supports platform apps.
  void Pin(ash::ShelfID id);

  // Unpins the specified id, closing if not running.
  void Unpin(ash::ShelfID id);

  // Returns true if the item identified by |id| is pinned.
  bool IsPinned(ash::ShelfID id);

  // Pins/unpins the specified id.
  void TogglePinned(ash::ShelfID id);

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  bool IsPinnable(ash::ShelfID id) const;

  // If there is no shelf item in the shelf for application |app_id|, one
  // gets created. The (existing or created) shelf items get then locked
  // against a users un-pinning removal.
  void LockV1AppWithID(const std::string& app_id);

  // A previously locked shelf item of type |app_id| gets unlocked. If the
  // lock count reaches 0 and the item is not pinned it will go away.
  void UnlockV1AppWithID(const std::string& app_id);

  // Requests that the shelf item controller specified by |id| open a new
  // instance of the app.  |event_flags| holds the flags of the event which
  // triggered this command.
  void Launch(ash::ShelfID id, int event_flags);

  // Closes the specified item.
  void Close(ash::ShelfID id);

  // Returns true if the specified item is open.
  bool IsOpen(ash::ShelfID id);

  // Returns true if the specified item is for a platform app.
  bool IsPlatformApp(ash::ShelfID id);

  // Opens a new instance of the application identified by |app_id|.
  // Used by the app-list, and by pinned-app shelf items.
  void LaunchApp(const std::string& app_id,
                 ash::LaunchSource source,
                 int event_flags);

  // If |app_id| is running, reactivates the app's most recently active window,
  // otherwise launches and activates the app.
  // Used by the app-list, and by pinned-app shelf items.
  void ActivateApp(const std::string& app_id,
                   ash::LaunchSource source,
                   int event_flags);

  // Returns the launch type of app for the specified id.
  extensions::LaunchType GetLaunchType(ash::ShelfID id);

  // Set the image for a specific shelf item (e.g. when set by the app).
  void SetLauncherItemImage(ash::ShelfID shelf_id, const gfx::ImageSkia& image);

  // Find out if the given application |id| is a windowed app item and not a
  // pinned item in the shelf.
  bool IsWindowedAppInLauncher(const std::string& app_id);

  // Updates the launch type of the app for the specified id to |launch_type|.
  void SetLaunchType(ash::ShelfID id, extensions::LaunchType launch_type);

  // Returns true if the user is currently logged in as a guest.
  // Makes virtual for unittest in LauncherContextMenuTest.
  virtual bool IsLoggedInAsGuest();

  // Invoked when user clicks on button in the shelf and there is no last
  // used window (or CTRL is held with the click).
  void CreateNewWindow();

  // Invoked when the user clicks on button in the shelf to create a new
  // incognito window.
  void CreateNewIncognitoWindow();

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  void PersistPinnedState();

  ash::ShelfModel* model();

  // Accessor to the currently loaded profile. Note that in multi profile use
  // cases this might change over time.
  Profile* profile();

  // Gets the shelf auto-hide behavior on |root_window|.
  ash::ShelfAutoHideBehavior GetShelfAutoHideBehavior(
      aura::Window* root_window) const;

  // Returns |true| if the user is allowed to modify the shelf auto-hide
  // behavior on |root_window|.
  bool CanUserModifyShelfAutoHideBehavior(aura::Window* root_window) const;

  // Toggles the shelf auto-hide behavior on |root_window|. Does nothing if the
  // user is not allowed to modify the auto-hide behavior.
  void ToggleShelfAutoHideBehavior(aura::Window* root_window);

  // Notify the controller that the state of an non platform app's tabs
  // have changed,
  void UpdateAppState(content::WebContents* contents, AppState app_state);

  // Returns ShelfID for |contents|. If |contents| is not an app or is not
  // pinned, returns the id of browser shrotcut.
  ash::ShelfID GetShelfIDForWebContents(content::WebContents* contents);

  // Limits application refocusing to urls that match |url| for |id|.
  void SetRefocusURLPatternForTest(ash::ShelfID id, const GURL& url);

  // Returns the extension identified by |app_id|.
  const extensions::Extension* GetExtensionForAppID(
      const std::string& app_id) const;

  // Activates a |window|. If |allow_minimize| is true and the system allows
  // it, the the window will get minimized instead.
  // Returns the action performed. Should be one of kNoAction,
  // kExistingWindowActivated, or kExistingWindowMinimized.
  ash::ShelfItemDelegate::PerformedAction ActivateWindowOrMinimizeIfActive(
      ui::BaseWindow* window,
      bool allow_minimize);

  // ash::ShelfDelegate:
  void OnShelfCreated(ash::Shelf* shelf) override;
  void OnShelfDestroyed(ash::Shelf* shelf) override;
  ash::ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  bool HasShelfIDToAppIDMapping(ash::ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ash::ShelfID id) override;
  bool GetAppIDForShelfIDConst(ash::ShelfID id, std::string* app_id) const;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

  // ash::ShelfItemDelegateManagerObserver:
  void OnSetShelfItemDelegate(ash::ShelfID id,
                              ash::ShelfItemDelegate* item_delegate) override;

  // ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override;
  void ShelfItemRemoved(int index, ash::ShelfID id) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override;
  void ShelfStatusChanged() override;

  // ash::ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;

  // ash::WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;

  // syncable_prefs::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  // AppSyncUIStateObserver:
  void OnAppSyncUIStatusChanged() override;

  // ExtensionEnableFlowDelegate:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // extensions::AppIconLoader:
  void SetAppImage(const std::string& app_id,
                   const gfx::ImageSkia& image) override;

  // ash::ShelfLayoutManagerObserver:
  void OnAutoHideBehaviorChanged(
      aura::Window* root_window,
      ash::ShelfAutoHideBehavior new_behavior) override;

  // Called when the active user has changed.
  void ActiveUserChanged(const std::string& user_email);

  // Called when a user got added to the session.
  void AdditionalUserAddedToSession(Profile* profile);

  // Get the list of all running incarnations of this item.
  // |event_flags| specifies the flags which were set by the event which
  // triggered this menu generation. It can be used to generate different lists.
  ChromeLauncherAppMenuItems GetApplicationList(const ash::ShelfItem& item,
                                                int event_flags);

  // Get the list of all tabs which belong to a certain application type.
  std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      const std::string& app_id);

  // Activates a specified shell application.
  void ActivateShellApp(const std::string& app_id, int index);

  // Checks if a given |web_contents| is known to be associated with an
  // application of type |app_id|.
  bool IsWebContentHandledByApplication(content::WebContents* web_contents,
                                        const std::string& app_id);

  // Check if the gMail app is loaded and it can handle the given web content.
  // This special treatment is required to address crbug.com/234268.
  bool ContentCanBeHandledByGmailApp(content::WebContents* web_contents);

  // Get the favicon for the application list entry for |web_contents|.
  // Note that for incognito windows the incognito icon will be returned.
  // If |web_contents| has not loaded, returns the default favicon.
  gfx::Image GetAppListIcon(content::WebContents* web_contents) const;

  // Get the title for the applicatoin list entry for |web_contents|.
  // If |web_contents| has not loaded, returns "Net Tab".
  base::string16 GetAppListTitle(content::WebContents* web_contents) const;

  // Returns the LauncherItemController of BrowserShortcut.
  BrowserShortcutLauncherItemController*
      GetBrowserShortcutLauncherItemController();

  LauncherItemController* GetLauncherItemController(const ash::ShelfID id);

  // Returns true if |browser| is owned by the active user.
  bool IsBrowserFromActiveUser(Browser* browser);

  // Check if the shelf visibility (location, visibility) will change with a new
  // user profile or not. However, since the full visibility calculation of the
  // shelf cannot be performed here, this is only a probability used for
  // animation predictions.
  bool ShelfBoundsChangesProbablyWithUser(aura::Window* root_window,
                                          const std::string& user_id) const;

  // Called when the user profile is fully loaded and ready to switch to.
  void OnUserProfileReadyToSwitch(Profile* profile);

  // Access to the BrowserStatusMonitor for tests.
  BrowserStatusMonitor* browser_status_monitor_for_test() {
    return browser_status_monitor_.get();
  }

  // Access to the AppWindowLauncherController for tests.
  AppWindowLauncherController* app_window_controller_for_test() {
    return app_window_controller_.get();
  }

  bool CanPin(const std::string& app_id);

 protected:
  // Creates a new app shortcut item and controller on the shelf at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  ash::ShelfID CreateAppShortcutLauncherItem(const std::string& app_id,
                                             int index);

  // Sets the AppTabHelper/AppIconLoader, taking ownership of the helper class.
  // These are intended for testing.
  void SetAppTabHelperForTest(AppTabHelper* helper);
  void SetAppIconLoaderForTest(extensions::AppIconLoader* loader);
  const std::string& GetAppIdFromShelfIdForTest(ash::ShelfID id);

  // Sets the ash::ShelfItemDelegateManager only for unittests and doesn't
  // take an ownership of it.
  void SetShelfItemDelegateManagerForTest(
      ash::ShelfItemDelegateManager* manager);

 private:
  friend class ChromeLauncherControllerTest;
  friend class ShelfAppBrowserTest;
  friend class LauncherPlatformAppBrowserTest;

  typedef std::map<ash::ShelfID, LauncherItemController*> IDToItemControllerMap;
  typedef std::map<content::WebContents*, std::string> WebContentsToAppIDMap;

  // Remembers / restores list of running applications.
  // Note that this order will neither be stored in the preference nor will it
  // remember the order of closed applications since it is only temporary.
  void RememberUnpinnedRunningApplicationOrder();
  void RestoreUnpinnedRunningApplicationOrder(const std::string& user_id);

  // Creates a new app shortcut item and controller on the shelf at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  ash::ShelfID CreateAppShortcutLauncherItemWithType(
      const std::string& app_id,
      int index,
      ash::ShelfItemType shelf_item_type);

  // Invoked when the associated browser or app is closed.
  void LauncherItemClosed(ash::ShelfID id);

  // Internal helpers for pinning and unpinning that handle both
  // client-triggered and internal pinning operations.
  void DoPinAppWithID(const std::string& app_id);
  void DoUnpinAppWithID(const std::string& app_id);

  // Pin a running app with |shelf_id| internally to |index|. It returns
  // the index where the item was pinned.
  int PinRunningAppInternal(int index, ash::ShelfID shelf_id);

  // Unpin a locked application. This is an internal call which converts the
  // model type of the given app index from a shortcut into an unpinned running
  // app.
  void UnpinRunningAppInternal(int index);

  // Re-syncs shelf model with prefs::kPinnedLauncherApps.
  void UpdateAppLaunchersFromPref();

  // Persists the shelf auto-hide behavior to prefs.
  void SetShelfAutoHideBehaviorPrefs(ash::ShelfAutoHideBehavior behavior,
                                     aura::Window* root_window);

  // Sets the shelf auto-hide behavior from prefs.
  void SetShelfAutoHideBehaviorFromPrefs();

  // Sets the shelf alignment from prefs.
  void SetShelfAlignmentFromPrefs();

  // Sets both of auto-hide behavior and alignment from prefs.
  void SetShelfBehaviorsFromPrefs();

#if defined(OS_CHROMEOS)
  // Sets whether the virtual keyboard is enabled from prefs.
  void SetVirtualKeyboardBehaviorFromPrefs();
#endif  // defined(OS_CHROMEOS)

  // Returns the shelf item status for the given |app_id|, which can be either
  // STATUS_ACTIVE (if the app is active), STATUS_RUNNING (if there is such an
  // app) or STATUS_CLOSED.
  ash::ShelfItemStatus GetAppState(const std::string& app_id);

  // Creates an app launcher to insert at |index|. Note that |index| may be
  // adjusted by the model to meet ordering constraints.
  // The |shelf_item_type| will be set into the ShelfModel.
  ash::ShelfID InsertAppLauncherItem(LauncherItemController* controller,
                                        const std::string& app_id,
                                        ash::ShelfItemStatus status,
                                        int index,
                                        ash::ShelfItemType shelf_item_type);

  // Enumerate all Web contents which match a given shortcut |controller|.
  std::vector<content::WebContents*> GetV1ApplicationsFromController(
      LauncherItemController* controller);

  // Create ShelfItem for Browser Shortcut.
  ash::ShelfID CreateBrowserShortcutLauncherItem();

  // Check if the given |web_contents| is in incognito mode.
  bool IsIncognito(const content::WebContents* web_contents) const;

  // Update browser shortcut's index.
  void PersistChromeItemIndex(int index);

  // Get browser shortcut's index from pref.
  int GetChromeIconIndexFromPref() const;

  // Depending on the provided flags, move either the chrome icon, the app icon
  // or none to the given |target_index|. The provided |chrome_index| and
  // |app_list_index| locations will get adjusted within this call to finalize
  // the action and to make sure that the other item can still be moved
  // afterwards (index adjustments).
  void MoveChromeOrApplistToFinalPosition(
      bool is_chrome,
      bool is_app_list,
      int target_index,
      int* chrome_index,
      int* app_list_index);

  // Finds the index of where to insert the next item.
  int FindInsertionPoint(bool is_app_list);

  // Get the browser shortcut's index in the shelf using the current's systems
  // configuration of pinned and known (but not running) apps.
  int GetChromeIconIndexForCreation();

  // Get the list of pinned programs from the preferences.
  std::vector<std::string> GetListOfPinnedAppsAndBrowser();

  // Close all windowed V1 applications of a certain extension which was already
  // deleted.
  void CloseWindowedAppsFromRemovedExtension(const std::string& app_id,
                                             const Profile* profile);

  // Set ShelfItemDelegate |item_delegate| for |id| and take an ownership.
  // TODO(simon.hong81): Make this take a scoped_ptr of |item_delegate|.
  void SetShelfItemDelegate(ash::ShelfID id,
                            ash::ShelfItemDelegate* item_delegate);

  // Attach to a specific profile.
  void AttachProfile(Profile* proifile);

  // Forget the current profile to allow attaching to a new one.
  void ReleaseProfile();

  static ChromeLauncherController* instance_;

  ash::ShelfModel* model_;

  ash::ShelfItemDelegateManager* item_delegate_manager_;

  // Profile used for prefs and loading extensions. This is NOT necessarily the
  // profile new windows are created with.
  Profile* profile_;

  // Controller items in this map are owned by |ShelfItemDelegateManager|.
  IDToItemControllerMap id_to_item_controller_map_;

  // Direct access to app_id for a web contents.
  WebContentsToAppIDMap web_contents_to_app_id_;

  // Used to track app windows.
  scoped_ptr<AppWindowLauncherController> app_window_controller_;

  // Used to get app info for tabs.
  scoped_ptr<AppTabHelper> app_tab_helper_;

  // Used to load the image for an app item.
  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  PrefChangeRegistrar pref_change_registrar_;

  AppSyncUIState* app_sync_ui_state_;

  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  // Shelves that are currently being observed.
  std::set<ash::Shelf*> shelves_;

  // The owned browser status monitor.
  scoped_ptr<BrowserStatusMonitor> browser_status_monitor_;

#if defined(OS_CHROMEOS)
  // A special observer class to detect user switches.
  scoped_ptr<ChromeLauncherControllerUserSwitchObserver> user_switch_observer_;
#endif

  // If true, incoming pinned state changes should be ignored.
  bool ignore_persist_pinned_state_change_;

  // The list of running & un-pinned applications for different users on hidden
  // desktops.
  typedef std::vector<std::string> RunningAppListIds;
  typedef std::map<std::string, RunningAppListIds> RunningAppListIdMap;
  RunningAppListIdMap last_used_running_application_order_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
