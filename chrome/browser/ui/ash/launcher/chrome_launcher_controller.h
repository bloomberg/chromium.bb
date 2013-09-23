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

#include "ash/display/display_controller.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_item_delegate.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"
#include "chrome/browser/ui/ash/app_sync_ui_state_observer.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "ui/aura/window_observer.h"

class AppSyncUIState;
class Browser;
class BrowserShortcutLauncherItemController;
class BrowserStatusMonitor;
class ExtensionEnableFlow;
class GURL;
class LauncherItemController;
class Profile;
class ShellWindowLauncherController;
class TabContents;

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;
}

namespace content {
class NotificationRegistrar;
class WebContents;
}

namespace ui {
class BaseWindow;
}

// A list of the elements which makes up a simple menu description.
typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

// A class which needs to be overwritten dependent on the used OS to moitor
// user switching.
class ChromeLauncherControllerUserSwitchObserver {
 public:
  ChromeLauncherControllerUserSwitchObserver() {}
  virtual ~ChromeLauncherControllerUserSwitchObserver() {}

 private:

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerUserSwitchObserver);
};

// ChromeLauncherController manages the launcher items needed for content
// windows. Launcher items have a type, an optional app id, and a controller.
// This incarnation groups running tabs/windows in application specific lists.
// * Browser app windows have BrowserLauncherItemController, owned by the
//   BrowserView instance.
// * App shell windows have ShellWindowLauncherItemController, owned by
//   ShellWindowLauncherController.
// * Shortcuts have no LauncherItemController.
// TODO(simon.hong81): Move LauncherItemDelegate out from
// ChromeLauncherController and makes separate subclass with it.
class ChromeLauncherController : public ash::LauncherDelegate,
                                 public ash::LauncherItemDelegate,
                                 public ash::LauncherModelObserver,
                                 public ash::ShellObserver,
                                 public ash::DisplayController::Observer,
                                 public content::NotificationObserver,
                                 public extensions::AppIconLoader::Delegate,
                                 public PrefServiceSyncableObserver,
                                 public AppSyncUIStateObserver,
                                 public ExtensionEnableFlowDelegate,
                                 public ash::ShelfLayoutManagerObserver {
 public:
  // Indicates if a launcher item is incognito or not.
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
    // no app.
    virtual std::string GetAppID(content::WebContents* tab) = 0;

    // Returns true if |id| is valid. Used during restore to ignore no longer
    // valid extensions.
    virtual bool IsValidID(const std::string& id) = 0;
  };

  ChromeLauncherController(Profile* profile, ash::LauncherModel* model);
  virtual ~ChromeLauncherController();

  // Initializes this ChromeLauncherController.
  void Init();

  // Creates an instance.
  static ChromeLauncherController* CreateInstance(Profile* profile,
                                                  ash::LauncherModel* model);

  // Returns the single ChromeLauncherController instance.
  static ChromeLauncherController* instance() { return instance_; }

  // Creates a new app item on the launcher for |controller|.
  ash::LauncherID CreateAppLauncherItem(LauncherItemController* controller,
                                        const std::string& app_id,
                                        ash::LauncherItemStatus status);

  // Updates the running status of an item. It will also update the status of
  // browsers launcher item if needed.
  void SetItemStatus(ash::LauncherID id, ash::LauncherItemStatus status);

  // Updates the controller associated with id (which should be a shortcut).
  // |controller| remains owned by caller.
  void SetItemController(ash::LauncherID id,
                         LauncherItemController* controller);

  // Closes or unpins the launcher item.
  void CloseLauncherItem(ash::LauncherID id);

  // Pins the specified id. Currently only supports platform apps.
  void Pin(ash::LauncherID id);

  // Unpins the specified id, closing if not running.
  void Unpin(ash::LauncherID id);

  // Returns true if the item identified by |id| is pinned.
  bool IsPinned(ash::LauncherID id);

  // Pins/unpins the specified id.
  void TogglePinned(ash::LauncherID id);

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  bool IsPinnable(ash::LauncherID id) const;

  // If there is no launcher item in the launcher for application |app_id|, one
  // gets created. The (existing or created) launcher items get then locked
  // against a users un-pinning removal.
  void LockV1AppWithID(const std::string& app_id);

  // A previously locked launcher item of type |app_id| gets unlocked. If the
  // lock count reaches 0 and the item is not pinned it will go away.
  void UnlockV1AppWithID(const std::string& app_id);

  // Requests that the launcher item controller specified by |id| open a new
  // instance of the app.  |event_flags| holds the flags of the event which
  // triggered this command.
  void Launch(ash::LauncherID id, int event_flags);

  // Closes the specified item.
  void Close(ash::LauncherID id);

  // Returns true if the specified item is open.
  bool IsOpen(ash::LauncherID id);

  // Returns true if the specified item is for a platform app.
  bool IsPlatformApp(ash::LauncherID id);

  // Opens a new instance of the application identified by |app_id|.
  // Used by the app-list, and by pinned-app launcher items.
  void LaunchApp(const std::string& app_id, int event_flags);

  // If |app_id| is running, reactivates the app's most recently active window,
  // otherwise launches and activates the app.
  // Used by the app-list, and by pinned-app launcher items.
  void ActivateApp(const std::string& app_id, int event_flags);

  // Returns the launch type of app for the specified id.
  extensions::ExtensionPrefs::LaunchType GetLaunchType(ash::LauncherID id);

  // Returns the id of the app for the specified tab.
  std::string GetAppID(content::WebContents* tab);

  // Set the image for a specific launcher item (e.g. when set by the app).
  void SetLauncherItemImage(ash::LauncherID launcher_id,
                            const gfx::ImageSkia& image);

  // Find out if the given application |id| is a windowed app item and not a
  // pinned item in the launcher.
  bool IsWindowedAppInLauncher(const std::string& app_id);

  // Updates the launche type of the app for the specified id to |launch_type|.
  void SetLaunchType(ash::LauncherID id,
                     extensions::ExtensionPrefs::LaunchType launch_type);

  // Returns true if the user is currently logged in as a guest.
  // Makes virtual for unittest in LauncherContextMenuTest.
  virtual bool IsLoggedInAsGuest();

  // Invoked when user clicks on button in the launcher and there is no last
  // used window (or CTRL is held with the click).
  void CreateNewWindow();

  // Invoked when the user clicks on button in the launcher to create a new
  // incognito window.
  void CreateNewIncognitoWindow();

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  void PersistPinnedState();

  ash::LauncherModel* model();

  // Accessor to the currently loaded profile. Note that in multi profile use
  // cases this might change over time.
  Profile* profile();

  // Gets the shelf auto-hide behavior on |root_window|.
  ash::ShelfAutoHideBehavior GetShelfAutoHideBehavior(
      aura::RootWindow* root_window) const;

  // Returns |true| if the user is allowed to modify the shelf auto-hide
  // behavior on |root_window|.
  bool CanUserModifyShelfAutoHideBehavior(aura::RootWindow* root_window) const;

  // Toggles the shelf auto-hide behavior on |root_window|. Does nothing if the
  // user is not allowed to modify the auto-hide behavior.
  void ToggleShelfAutoHideBehavior(aura::RootWindow* root_window);

  // The tab no longer represents its previously identified application.
  void RemoveTabFromRunningApp(content::WebContents* tab,
                               const std::string& app_id);

  // Notify the controller that the state of an non platform app's tabs
  // have changed,
  void UpdateAppState(content::WebContents* contents, AppState app_state);

  // Limits application refocusing to urls that match |url| for |id|.
  void SetRefocusURLPatternForTest(ash::LauncherID id, const GURL& url);

  // Returns the extension identified by |app_id|.
  const extensions::Extension* GetExtensionForAppID(
      const std::string& app_id) const;

  // Activates a |window|. If |allow_minimize| is true and the system allows
  // it, the the window will get minimized instead.
  void ActivateWindowOrMinimizeIfActive(ui::BaseWindow* window,
                                        bool allow_minimize);

  // ash::LauncherDelegate overrides:
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual void OnLauncherCreated(ash::Launcher* launcher) OVERRIDE;
  virtual void OnLauncherDestroyed(ash::Launcher* launcher) OVERRIDE;
  virtual ash::LauncherID GetLauncherIDForAppID(
      const std::string& app_id) OVERRIDE;
  virtual const std::string& GetAppIDForLauncherID(ash::LauncherID id) OVERRIDE;
  virtual void PinAppWithID(const std::string& app_id) OVERRIDE;
  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE;
  virtual bool CanPin() const OVERRIDE;
  virtual void UnpinAppWithID(const std::string& app_id) OVERRIDE;

  // ash::LauncherItemDelegate overrides:
  virtual void ItemSelected(const ash::LauncherItem& item,
                           const ui::Event& event) OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item, aura::RootWindow* root) OVERRIDE;
  virtual ash::LauncherMenuModel* CreateApplicationMenu(
      const ash::LauncherItem& item,
      int event_flags) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;
  virtual bool ShouldShowTooltip(const ash::LauncherItem& item) OVERRIDE;

  // ash::LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;
  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE;
  virtual void LauncherStatusChanged() OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnShelfAlignmentChanged(aura::RootWindow* root_window) OVERRIDE;

  // ash::DisplayController::Observer overrides:
  virtual void OnDisplayConfigurationChanging() OVERRIDE;
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  // PrefServiceSyncableObserver overrides:
  virtual void OnIsSyncingChanged() OVERRIDE;

  // AppSyncUIStateObserver overrides:
  virtual void OnAppSyncUIStatusChanged() OVERRIDE;

  // ExtensionEnableFlowDelegate overrides:
  virtual void ExtensionEnableFlowFinished() OVERRIDE;
  virtual void ExtensionEnableFlowAborted(bool user_initiated) OVERRIDE;

  // extensions::AppIconLoader overrides:
  virtual void SetAppImage(const std::string& app_id,
                           const gfx::ImageSkia& image) OVERRIDE;

  // ash::ShelfLayoutManagerObserver overrides:
  virtual void OnAutoHideBehaviorChanged(
      aura::RootWindow* root_window,
      ash::ShelfAutoHideBehavior new_behavior) OVERRIDE;

  // Called when the active user has changed.
  void ActiveUserChanged(const std::string& user_email);

  // Get the list of all running incarnations of this item.
  // |event_flags| specifies the flags which were set by the event which
  // triggered this menu generation. It can be used to generate different lists.
  ChromeLauncherAppMenuItems GetApplicationList(const ash::LauncherItem& item,
                                                int event_flags);

  // Get the list of all tabs which belong to a certain application type.
  std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      std::string app_id);

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
  string16 GetAppListTitle(content::WebContents* web_contents) const;

  // Returns the LauncherItemController of BrowserShortcut.
  BrowserShortcutLauncherItemController*
      GetBrowserShortcutLauncherItemController();

 protected:
  // Creates a new app shortcut item and controller on the launcher at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  ash::LauncherID CreateAppShortcutLauncherItem(const std::string& app_id,
                                                int index);

  // Sets the AppTabHelper/AppIconLoader, taking ownership of the helper class.
  // These are intended for testing.
  void SetAppTabHelperForTest(AppTabHelper* helper);
  void SetAppIconLoaderForTest(extensions::AppIconLoader* loader);
  const std::string& GetAppIdFromLauncherIdForTest(ash::LauncherID id);

 private:
  friend class ChromeLauncherControllerTest;
  friend class LauncherAppBrowserTest;
  friend class LauncherPlatformAppBrowserTest;

  typedef std::map<ash::LauncherID, LauncherItemController*>
          IDToItemControllerMap;
  typedef std::list<content::WebContents*> WebContentsList;
  typedef std::map<std::string, WebContentsList> AppIDToWebContentsListMap;
  typedef std::map<content::WebContents*, std::string> WebContentsToAppIDMap;

  // Creates a new app shortcut item and controller on the launcher at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  ash::LauncherID CreateAppShortcutLauncherItemWithType(
      const std::string& app_id,
      int index,
      ash::LauncherItemType launcher_item_type);

  // Returns the profile used for new windows.
  Profile* GetProfileForNewWindows();

  // Invoked when the associated browser or app is closed.
  void LauncherItemClosed(ash::LauncherID id);

  // Internal helpers for pinning and unpinning that handle both
  // client-triggered and internal pinning operations.
  void DoPinAppWithID(const std::string& app_id);
  void DoUnpinAppWithID(const std::string& app_id);

  // Pin a running app with |launcher_id| internally to |index|. It returns
  // the index where the item was pinned.
  int PinRunningAppInternal(int index, ash::LauncherID launcher_id);

  // Unpin a locked application. This is an internal call which converts the
  // model type of the given app index from a shortcut into an unpinned running
  // app.
  void UnpinRunningAppInternal(int index);

  // Re-syncs launcher model with prefs::kPinnedLauncherApps.
  void UpdateAppLaunchersFromPref();

  // Persists the shelf auto-hide behavior to prefs.
  void SetShelfAutoHideBehaviorPrefs(ash::ShelfAutoHideBehavior behavior,
                                     aura::RootWindow* root_window);

  // Sets the shelf auto-hide behavior from prefs.
  void SetShelfAutoHideBehaviorFromPrefs();

  // Sets the shelf alignment from prefs.
  void SetShelfAlignmentFromPrefs();

  // Sets both of auto-hide behavior and alignment from prefs.
  void SetShelfBehaviorsFromPrefs();

  // Returns the most recently active web contents for an app.
  content::WebContents* GetLastActiveWebContents(const std::string& app_id);

  // Creates an app launcher to insert at |index|. Note that |index| may be
  // adjusted by the model to meet ordering constraints.
  // The |launcher_item_type| will be set into the LauncherModel.
  ash::LauncherID InsertAppLauncherItem(
      LauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status,
      int index,
      ash::LauncherItemType launcher_item_type);

  bool HasItemController(ash::LauncherID id) const;

  // Enumerate all Web contents which match a given shortcut |controller|.
  std::vector<content::WebContents*> GetV1ApplicationsFromController(
      LauncherItemController* controller);

  // Create LauncherItem for Browser Shortcut.
  ash::LauncherID CreateBrowserShortcutLauncherItem();

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
  void CloseWindowedAppsFromRemovedExtension(const std::string& app_id);

  // Register LauncherItemDelegate.
  void RegisterLauncherItemDelegate();

  // Attach to a specific profile.
  void AttachProfile(Profile* proifile);

  // Forget the current profile to allow attaching to a new one.
  void ReleaseProfile();

  static ChromeLauncherController* instance_;

  ash::LauncherModel* model_;

  // Profile used for prefs and loading extensions. This is NOT necessarily the
  // profile new windows are created with.
  Profile* profile_;

  IDToItemControllerMap id_to_item_controller_map_;

  // Maintains activation order of web contents for each app.
  AppIDToWebContentsListMap app_id_to_web_contents_list_;

  // Direct access to app_id for a web contents.
  WebContentsToAppIDMap web_contents_to_app_id_;

  // Used to track shell windows.
  scoped_ptr<ShellWindowLauncherController> shell_window_controller_;

  // Used to get app info for tabs.
  scoped_ptr<AppTabHelper> app_tab_helper_;

  // Used to load the image for an app item.
  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  content::NotificationRegistrar notification_registrar_;

  PrefChangeRegistrar pref_change_registrar_;

  AppSyncUIState* app_sync_ui_state_;

  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  // Launchers that are currently being observed.
  std::set<ash::Launcher*> launchers_;

  // The owned browser shortcut item.
  scoped_ptr<BrowserShortcutLauncherItemController> browser_item_controller_;

  // The owned browser status monitor.
  scoped_ptr<BrowserStatusMonitor> browser_status_monitor_;

  // A special observer class to detect user switches.
  scoped_ptr<ChromeLauncherControllerUserSwitchObserver> user_switch_observer_;

  // If true, incoming pinned state changes should be ignored.
  bool ignore_persist_pinned_state_change_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
