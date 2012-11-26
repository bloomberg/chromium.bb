// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_

#include <list>
#include <map>
#include <string>

#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shell_observer.h"
#include "ash/wm/shelf_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/prefs/public/pref_observer.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service_observer.h"
#include "chrome/browser/ui/ash/app_sync_ui_state_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/window_observer.h"

class AppSyncUIState;
class Browser;
class BrowserLauncherItemControllerTest;
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
class WebContents;
}

// ChromeLauncherController manages the launcher items needed for content
// windows. Launcher items have a type, an optional app id, and a controller.
// * Tabbed browsers and browser app windows have BrowserLauncherItemController,
//   owned by the BrowserView instance.
// * App shell windows have ShellWindowLauncherItemController, owned by
//   ShellWindowLauncherController.
// * Shortcuts have no LauncherItemController.
class ChromeLauncherController
    : public ash::LauncherDelegate,
      public ash::LauncherModelObserver,
      public ash::ShellObserver,
      public content::NotificationObserver,
      public PrefObserver,
      public PrefServiceObserver,
      public AppSyncUIStateObserver {
 public:
  // Indicates if a launcher item is incognito or not.
  enum IncognitoState {
    STATE_INCOGNITO,
    STATE_NOT_INCOGNITO,
  };

  // Used to update the state of non plaform apps, as tab contents change.
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

  // Interface used to load app icons. This is in it's own class so that it can
  // be mocked.
  class AppIconLoader {
   public:
    virtual ~AppIconLoader() {}

    // Fetches the image for the specified id. When done (which may be
    // synchronous), this should invoke SetAppImage() on the LauncherUpdater.
    virtual void FetchImage(const std::string& id) = 0;

    // Clears the image for the specified id.
    virtual void ClearImage(const std::string& id) = 0;
  };

  ChromeLauncherController(Profile* profile, ash::LauncherModel* model);
  virtual ~ChromeLauncherController();

  // Initializes this ChromeLauncherController.
  void Init();

  // Returns the single ChromeLauncherController instance.
  static ChromeLauncherController* instance() { return instance_; }

  // Creates a new tabbed item on the launcher for |controller|.
  ash::LauncherID CreateTabbedLauncherItem(
      LauncherItemController* controller,
      IncognitoState is_incognito,
      ash::LauncherItemStatus status);

  // Creates a new app item on the launcher for |controller|.
  ash::LauncherID CreateAppLauncherItem(
      LauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status);

  // Updates the running status of an item.
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

  ash::LauncherID GetLauncherIDForAppID(const std::string& app_id);
  std::string GetAppIDForLauncherID(ash::LauncherID id);

  // Sets the image for an app tab. This is intended to be invoked from the
  // AppIconLoader.
  void SetAppImage(const std::string& app_id, const gfx::ImageSkia& image);

  // Returns true if a pinned launcher item with given |app_id| could be found.
  bool IsAppPinned(const std::string& app_id);

  // Pins an app with |app_id| to launcher. If there is a running instance in
  // launcher, the running instance is pinned. If there is no running instance,
  // a new launcher item is created and pinned.
  void PinAppWithID(const std::string& app_id);

  // Updates the launche type of the app for the specified id to |launch_type|.
  void SetLaunchType(ash::LauncherID id,
                     extensions::ExtensionPrefs::LaunchType launch_type);

  // Unpins any app items whose id is |app_id|.
  void UnpinAppsWithID(const std::string& app_id);

  // Returns true if the user is currently logged in as a guest.
  bool IsLoggedInAsGuest();

  // Invoked when user clicks on button in the launcher and there is no last
  // used window (or CTRL is held with the click).
  void CreateNewWindow();

  // Invoked when the user clicks on button in the launcher to create a new
  // incognito window.
  void CreateNewIncognitoWindow();

  // Checks whether the user is allowed to pin apps. Pinning may be disallowed
  // by policy in case there is a pre-defined set of pinned apps.
  bool CanPin() const;

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  void PersistPinnedState();

  ash::LauncherModel* model() { return model_; }

  Profile* profile() { return profile_; }

  void SetAutoHideBehavior(ash::ShelfAutoHideBehavior behavior,
                           aura::RootWindow* root_window);

  // The tab no longer represents its previously identified application.
  void RemoveTabFromRunningApp(TabContents* tab, const std::string& app_id);

  // Notify the controller that the state of an non platform app's tabs
  // have changed,
  void UpdateAppState(content::WebContents* contents, AppState app_state);

  // Limits application refocusing to urls that match |url| for |id|.
  void SetRefocusURLPattern(ash::LauncherID id, const GURL& url);

  // Returns the extension identified by |app_id|.
  const extensions::Extension* GetExtensionForAppID(const std::string& app_id);

  // ash::LauncherDelegate overrides:
  virtual void OnBrowserShortcutClicked(int event_flags) OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item,
                           int event_flags) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item, aura::RootWindow* root) OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;

  // ash::LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;
  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE;
  virtual void LauncherStatusChanged() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from PrefObserver:
  virtual void OnPreferenceChanged(PrefServiceBase* service,
                                   const std::string& pref_name) OVERRIDE;

  // Overridden from ash::ShellObserver:
  virtual void OnShelfAlignmentChanged() OVERRIDE;

  // Overridden from PrefServiceObserver:
  virtual void OnIsSyncingChanged() OVERRIDE;

  // Overridden from AppSyncUIStateObserver
  virtual void OnAppSyncUIStatusChanged() OVERRIDE;

 private:
  friend class BrowserLauncherItemControllerTest;
  friend class ChromeLauncherControllerTest;
  friend class LauncherAppBrowserTest;
  friend class LauncherPlatformAppBrowserTest;

  typedef std::map<ash::LauncherID, LauncherItemController*>
          IDToItemControllerMap;
  typedef std::list<TabContents*> TabContentsList;
  typedef std::map<std::string, TabContentsList> AppIDToTabContentsListMap;
  typedef std::map<TabContents*, std::string> TabContentsToAppIDMap;

  // Sets the AppTabHelper/AppIconLoader, taking ownership of the helper class.
  // These are intended for testing.
  void SetAppTabHelperForTest(AppTabHelper* helper);
  void SetAppIconLoaderForTest(AppIconLoader* loader);

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

  // Sets the shelf auto-hide behavior from prefs.
  void SetShelfAutoHideBehaviorFromPrefs();

  // Sets the shelf alignment from prefs.
  void SetShelfAlignmentFromPrefs();

  // Returns the most recently active tab contents for an app.
  TabContents* GetLastActiveTabContents(const std::string& app_id);

  // Creates an app launcher to insert at |index|. Note that |index| may be
  // adjusted by the model to meet ordering constraints.
  ash::LauncherID InsertAppLauncherItem(
      LauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status,
      int index);

  // Creates a new app shortcut item and controller on the launcher at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  ash::LauncherID CreateAppShortcutLauncherItem(const std::string& app_id,
                                                int index);

  bool HasItemController(ash::LauncherID id) const;

  static ChromeLauncherController* instance_;

  ash::LauncherModel* model_;

  // Profile used for prefs and loading extensions. This is NOT necessarily the
  // profile new windows are created with.
  Profile* profile_;

  IDToItemControllerMap id_to_item_controller_map_;

  // Maintains activation order of tab contents for each app.
  AppIDToTabContentsListMap app_id_to_tab_contents_list_;

  // Direct access to app_id for a tab contents.
  TabContentsToAppIDMap tab_contents_to_app_id_;

  // Used to track shell windows.
  scoped_ptr<ShellWindowLauncherController> shell_window_controller_;

  // Used to get app info for tabs.
  scoped_ptr<AppTabHelper> app_tab_helper_;

  // Used to load the image for an app item.
  scoped_ptr<AppIconLoader> app_icon_loader_;

  content::NotificationRegistrar notification_registrar_;

  PrefChangeRegistrar pref_change_registrar_;

  AppSyncUIState* app_sync_ui_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
