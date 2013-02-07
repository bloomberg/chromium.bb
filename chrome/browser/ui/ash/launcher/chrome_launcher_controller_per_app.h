// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_PER_APP_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_PER_APP_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf_types.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"
#include "chrome/browser/ui/ash/app_sync_ui_state_observer.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shell_window_launcher_controller.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/window_observer.h"

class AppSyncUIState;
class Browser;
class BrowserLauncherItemControllerTest;
class ExtensionEnableFlow;
class LauncherItemController;
class Profile;
class TabContents;

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

// ChromeLauncherControllerPerApp manages the launcher items needed for content
// content windows. Launcher items have a type, an optional app id, and a
// controller. This incarnation groups running tabs/windows in application
// specific lists.
// * Tabbed browsers and browser app windows have BrowserLauncherItemController,
//   owned by the BrowserView instance.
// * App shell windows have ShellWindowLauncherItemController, owned by
//   ShellWindowLauncherController.
// * Shortcuts have no LauncherItemController.
class ChromeLauncherControllerPerApp : public ash::LauncherModelObserver,
                                       public ash::ShellObserver,
                                       public ChromeLauncherController,
                                       public content::NotificationObserver,
                                       public PrefServiceSyncableObserver,
                                       public AppSyncUIStateObserver,
                                       public ExtensionEnableFlowDelegate,
                                       public chrome::BrowserListObserver {
 public:
  ChromeLauncherControllerPerApp(Profile* profile, ash::LauncherModel* model);
  virtual ~ChromeLauncherControllerPerApp();

  // ChromeLauncherController overrides:

  // Initializes this ChromeLauncherControllerPerApp.
  virtual void Init() OVERRIDE;

  // Returns the new per application interface of the given launcher. If it is
  // a per browser (old) controller, it will return NULL;
  // TODO(skuhne): Remove when we rip out the old launcher.
  virtual ChromeLauncherControllerPerApp* GetPerAppInterface() OVERRIDE;

  // Creates a new tabbed item on the launcher for |controller|.
  virtual ash::LauncherID CreateTabbedLauncherItem(
      LauncherItemController* controller,
      IncognitoState is_incognito,
      ash::LauncherItemStatus status) OVERRIDE;

  // Creates a new app item on the launcher for |controller|.
  virtual ash::LauncherID CreateAppLauncherItem(
      LauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status) OVERRIDE;

  // Updates the running status of an item. It will also update the status of
  // browsers launcher item if needed.
  virtual void SetItemStatus(ash::LauncherID id,
                             ash::LauncherItemStatus status) OVERRIDE;

  // Updates the controller associated with id (which should be a shortcut).
  // |controller| remains owned by caller.
  virtual void SetItemController(ash::LauncherID id,
                                 LauncherItemController* controller) OVERRIDE;

  // Closes or unpins the launcher item.
  virtual void CloseLauncherItem(ash::LauncherID id) OVERRIDE;

  // Pins the specified id. Currently only supports platform apps.
  virtual void Pin(ash::LauncherID id) OVERRIDE;

  // Unpins the specified id, closing if not running.
  virtual void Unpin(ash::LauncherID id) OVERRIDE;

  // Returns true if the item identified by |id| is pinned.
  virtual bool IsPinned(ash::LauncherID id) OVERRIDE;

  // Pins/unpins the specified id.
  virtual void TogglePinned(ash::LauncherID id) OVERRIDE;

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  virtual bool IsPinnable(ash::LauncherID id) const OVERRIDE;

  // Requests that the launcher item controller specified by |id| open a new
  // instance of the app.  |event_flags| holds the flags of the event which
  // triggered this command.
  virtual void Launch(ash::LauncherID id, int event_flags) OVERRIDE;

  // Closes the specified item.
  virtual void Close(ash::LauncherID id) OVERRIDE;

  // Returns true if the specified item is open.
  virtual bool IsOpen(ash::LauncherID id) OVERRIDE;

  // Returns true if the specified item is for a platform app.
  virtual bool IsPlatformApp(ash::LauncherID id) OVERRIDE;

  // Opens a new instance of the application identified by |app_id|.
  // Used by the app-list, and by pinned-app launcher items.
  virtual void LaunchApp(const std::string& app_id, int event_flags) OVERRIDE;

  // If |app_id| is running, reactivates the app's most recently active window,
  // otherwise launches and activates the app.
  // Used by the app-list, and by pinned-app launcher items.
  virtual void ActivateApp(const std::string& app_id, int event_flags) OVERRIDE;

  // Returns the launch type of app for the specified id.
  virtual extensions::ExtensionPrefs::LaunchType GetLaunchType(
      ash::LauncherID id) OVERRIDE;

  // Returns the id of the app for the specified tab.
  virtual std::string GetAppID(content::WebContents* tab) OVERRIDE;

  // Returns the |LauncherModel|'s ID or 0 if the AppId was not found.
  virtual ash::LauncherID GetLauncherIDForAppID(
      const std::string& app_id) OVERRIDE;
  virtual std::string GetAppIDForLauncherID(ash::LauncherID id) OVERRIDE;

  // Set the image for a specific launcher item (e.g. when set by the app).
  virtual void SetLauncherItemImage(ash::LauncherID launcher_id,
                                    const gfx::ImageSkia& image) OVERRIDE;

  // Returns true if a pinned launcher item with given |app_id| could be found.
  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE;

  // Pins an app with |app_id| to launcher. If there is a running instance in
  // launcher, the running instance is pinned. If there is no running instance,
  // a new launcher item is created and pinned.
  virtual void PinAppWithID(const std::string& app_id) OVERRIDE;

  // Updates the launche type of the app for the specified id to |launch_type|.
  virtual void SetLaunchType(
      ash::LauncherID id,
      extensions::ExtensionPrefs::LaunchType launch_type) OVERRIDE;

  // Unpins any app items whose id is |app_id|.
  virtual void UnpinAppsWithID(const std::string& app_id) OVERRIDE;

  // Returns true if the user is currently logged in as a guest.
  virtual bool IsLoggedInAsGuest() OVERRIDE;

  // Invoked when user clicks on button in the launcher and there is no last
  // used window (or CTRL is held with the click).
  virtual void CreateNewWindow() OVERRIDE;

  // Invoked when the user clicks on button in the launcher to create a new
  // incognito window.
  virtual void CreateNewIncognitoWindow() OVERRIDE;

  // Checks whether the user is allowed to pin apps. Pinning may be disallowed
  // by policy in case there is a pre-defined set of pinned apps.
  virtual bool CanPin() const OVERRIDE;

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  virtual void PersistPinnedState() OVERRIDE;

  virtual ash::LauncherModel* model() OVERRIDE;

  virtual Profile* profile() OVERRIDE;

  // Gets the shelf auto-hide behavior on |root_window|.
  virtual ash::ShelfAutoHideBehavior GetShelfAutoHideBehavior(
      aura::RootWindow* root_window) const OVERRIDE;

  // Returns |true| if the user is allowed to modify the shelf auto-hide
  // behavior on |root_window|.
  virtual bool CanUserModifyShelfAutoHideBehavior(
      aura::RootWindow* root_window) const OVERRIDE;

  // Toggles the shelf auto-hide behavior on |root_window|. Does nothing if the
  // user is not allowed to modify the auto-hide behavior.
  virtual void ToggleShelfAutoHideBehavior(
      aura::RootWindow* root_window) OVERRIDE;

  // The tab no longer represents its previously identified application.
  virtual void RemoveTabFromRunningApp(content::WebContents* tab,
                                       const std::string& app_id) OVERRIDE;

  // Notify the controller that the state of an non platform app's tabs
  // have changed,
  virtual void UpdateAppState(content::WebContents* contents,
                              AppState app_state) OVERRIDE;

  // Limits application refocusing to urls that match |url| for |id|.
  virtual void SetRefocusURLPatternForTest(ash::LauncherID id,
                                           const GURL& url) OVERRIDE;

  // Returns the extension identified by |app_id|.
  virtual const extensions::Extension* GetExtensionForAppID(
      const std::string& app_id) OVERRIDE;

  // ash::LauncherDelegate overrides:
  virtual void OnBrowserShortcutClicked(int event_flags) OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item,
                           const ui::Event& event) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item, aura::RootWindow* root) OVERRIDE;
  virtual ui::MenuModel* CreateApplicationMenu(
      const ash::LauncherItem& item) OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;

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

  // PrefServiceSyncableObserver overrides:
  virtual void OnIsSyncingChanged() OVERRIDE;

  // AppSyncUIStateObserver overrides:
  virtual void OnAppSyncUIStatusChanged() OVERRIDE;

  // ExtensionEnableFlowDelegate overrides:
  virtual void ExtensionEnableFlowFinished() OVERRIDE;
  virtual void ExtensionEnableFlowAborted(bool user_initiated) OVERRIDE;

  // ash::AppIconLoader overrides:
  virtual void SetAppImage(const std::string& app_id,
                           const gfx::ImageSkia& image) OVERRIDE;

  // Get the list of all running incarnations of this item.
  ChromeLauncherAppMenuItems GetApplicationList(const ash::LauncherItem& item);

  // Get the list of all tabs which belong to a certain application type.
  std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      std::string app_id);

  // Activates a specified shell application.
  void ActivateShellApp(const std::string& app_id, int index);

  // Checks if a given |web_contents| is known to be associated with an
  // application of type |app_id|.
  bool IsWebContentHandledByApplication(content::WebContents* web_contents,
                                        const std::string& app_id);

  // Get the favicon for the application list by giving the |web_content|.
  // Note that for incognito windows the incognito icon will be returned.
  gfx::Image GetAppListIcon(content::WebContents* web_contents) const;

  // Overridden from chrome::BrowserListObserver.
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

 protected:
  // ChromeLauncherController overrides:

  // Creates a new app shortcut item and controller on the launcher at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  virtual ash::LauncherID CreateAppShortcutLauncherItem(
      const std::string& app_id,
      int index) OVERRIDE;

  // Sets the AppTabHelper/AppIconLoader, taking ownership of the helper class.
  // These are intended for testing.
  virtual void SetAppTabHelperForTest(AppTabHelper* helper) OVERRIDE;
  virtual void SetAppIconLoaderForTest(ash::AppIconLoader* loader) OVERRIDE;
  virtual const std::string& GetAppIdFromLauncherIdForTest(
      ash::LauncherID id) OVERRIDE;

 private:
  friend class ChromeLauncherControllerPerAppTest;
  friend class LauncherPerAppAppBrowserTest;

  // Updates the activation state of the Broswer item.
  void UpdateBrowserItemStatus();

  typedef std::map<ash::LauncherID, LauncherItemController*>
          IDToItemControllerMap;
  typedef std::list<content::WebContents*> WebContentsList;
  typedef std::map<std::string, WebContentsList> AppIDToWebContentsListMap;
  typedef std::map<content::WebContents*, std::string> WebContentsToAppIDMap;

  // Returns the profile used for new windows.
  Profile* GetProfileForNewWindows();

  // Invoked when the associated browser or app is closed.
  void LauncherItemClosed(ash::LauncherID id);

  // Internal helpers for pinning and unpinning that handle both
  // client-triggered and internal pinning operations.
  void DoPinAppWithID(const std::string& app_id);
  void DoUnpinAppsWithID(const std::string& app_id);

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
  ash::LauncherID InsertAppLauncherItem(
      LauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status,
      int index);

  bool HasItemController(ash::LauncherID id) const;

  // Enumerate all Web contents which match a given shortcut |controller|.
  std::vector<content::WebContents*> GetV1ApplicationsFromController(
      LauncherItemController* controller);

  // Returns the list of all browsers runing.
  // TODO(skuhne): Move to wherever the BrowserLauncherItemController
  // functionality moves to.
  ChromeLauncherAppMenuItems GetBrowserApplicationList();

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
  scoped_ptr<ash::AppIconLoader> app_icon_loader_;

  content::NotificationRegistrar notification_registrar_;

  PrefChangeRegistrar pref_change_registrar_;

  AppSyncUIState* app_sync_ui_state_;

  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerPerApp);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_PER_APP_H_
