// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_

#include <string>

#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_types.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/browser/extensions/extension_prefs.h"

class BrowserLauncherItemControllerTest;
class LauncherItemController;
class Profile;
class ChromeLauncherAppMenuItem;
class ChromeLauncherControllerPerApp;

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;
class RootWindow;
}

namespace content {
class WebContents;
}

// A list of the elements which makes up a simple menu description.
typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

// ChromeLauncherController manages the launcher items needed for content
// windows. Launcher items have a type, an optional app id, and a controller.
// ChromeLauncherController will furthermore create the particular
// implementation of interest - either sorting by application (new) or sorting
// by browser (old).
// * Tabbed browsers and browser app windows have BrowserLauncherItemController,
//   owned by the BrowserView instance.
// * App shell windows have ShellWindowLauncherItemController, owned by
//   ShellWindowLauncherController.
// * Shortcuts have no LauncherItemController.
class ChromeLauncherController
    : public ash::LauncherDelegate,
      public extensions::AppIconLoader::Delegate {
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

  ChromeLauncherController() {}
  virtual ~ChromeLauncherController();

  // Initializes this ChromeLauncherController.
  virtual void Init() = 0;

  // Returns the new per application interface of the given launcher. If it is
  // a per browser (old) controller, it will return NULL;
  // TODO(skuhne): Remove when we rip out the old launcher.
  virtual ChromeLauncherControllerPerApp* GetPerAppInterface() = 0;

  // Creates an instance.
  static ChromeLauncherController* CreateInstance(Profile* profile,
                                                  ash::LauncherModel* model);

  // Returns the single ChromeLauncherController instance.
  static ChromeLauncherController* instance() { return instance_; }

  // Creates a new tabbed item on the launcher for |controller|.
  virtual ash::LauncherID CreateTabbedLauncherItem(
      LauncherItemController* controller,
      IncognitoState is_incognito,
      ash::LauncherItemStatus status) = 0;

  // Creates a new app item on the launcher for |controller|.
  virtual ash::LauncherID CreateAppLauncherItem(
      LauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status) = 0;

  // Updates the running status of an item.
  virtual void SetItemStatus(ash::LauncherID id,
                             ash::LauncherItemStatus status) = 0;

  // Updates the controller associated with id (which should be a shortcut).
  // |controller| remains owned by caller.
  virtual void SetItemController(ash::LauncherID id,
                                 LauncherItemController* controller) = 0;

  // Closes or unpins the launcher item.
  virtual void CloseLauncherItem(ash::LauncherID id) = 0;

  // Pins the specified id. Currently only supports platform apps.
  virtual void Pin(ash::LauncherID id) = 0;

  // Unpins the specified id, closing if not running.
  virtual void Unpin(ash::LauncherID id) = 0;

  // Returns true if the item identified by |id| is pinned.
  virtual bool IsPinned(ash::LauncherID id) = 0;

  // Pins/unpins the specified id.
  virtual void TogglePinned(ash::LauncherID id) = 0;

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  virtual bool IsPinnable(ash::LauncherID id) const = 0;

  // If there is no launcher item in the launcher for application |app_id|, one
  // gets created. The (existing or created) launcher items get then locked
  // against a users un-pinning removal.
  virtual void LockV1AppWithID(const std::string& app_id) = 0;

  // A previously locked launcher item of type |app_id| gets unlocked. If the
  // lock count reaches 0 and the item is not pinned it will go away.
  virtual void UnlockV1AppWithID(const std::string& app_id) = 0;

  // Requests that the launcher item controller specified by |id| open a new
  // instance of the app.  |event_flags| holds the flags of the event which
  // triggered this command.
  virtual void Launch(ash::LauncherID id, int event_flags) = 0;

  // Closes the specified item.
  virtual void Close(ash::LauncherID id) = 0;

  // Returns true if the specified item is open.
  virtual bool IsOpen(ash::LauncherID id) = 0;

  // Returns true if the specified item is for a platform app.
  virtual bool IsPlatformApp(ash::LauncherID id) = 0;

  // Opens a new instance of the application identified by |app_id|.
  // Used by the app-list, and by pinned-app launcher items.
  virtual void LaunchApp(const std::string& app_id, int event_flags) = 0;

  // If |app_id| is running, reactivates the app's most recently active window,
  // otherwise launches and activates the app.
  // Used by the app-list, and by pinned-app launcher items.
  virtual void ActivateApp(const std::string& app_id, int event_flags) = 0;

  // Returns the launch type of app for the specified id.
  virtual extensions::ExtensionPrefs::LaunchType GetLaunchType(
      ash::LauncherID id) = 0;

  // Returns the id of the app for the specified tab.
  virtual std::string GetAppID(content::WebContents* tab) = 0;

  // Returns the launcherID of the first non-panel item whose app_id
  // matches |app_id| or 0 if none match.
  virtual ash::LauncherID GetLauncherIDForAppID(const std::string& app_id) = 0;

  // Returns the id of the app for the specified id (which must exist).
  virtual std::string GetAppIDForLauncherID(ash::LauncherID id) = 0;

  // Set the image for a specific launcher item (e.g. when set by the app).
  virtual void SetLauncherItemImage(ash::LauncherID launcher_id,
                                    const gfx::ImageSkia& image) = 0;

  // Returns true if a pinned launcher item with given |app_id| could be found.
  virtual bool IsAppPinned(const std::string& app_id) = 0;

  // Pins an app with |app_id| to launcher. If there is a running instance in
  // launcher, the running instance is pinned. If there is no running instance,
  // a new launcher item is created and pinned.
  virtual void PinAppWithID(const std::string& app_id) = 0;

  // Updates the launche type of the app for the specified id to |launch_type|.
  virtual void SetLaunchType(
      ash::LauncherID id,
      extensions::ExtensionPrefs::LaunchType launch_type) = 0;

  // Unpins any app items whose id is |app_id|.
  virtual void UnpinAppsWithID(const std::string& app_id) = 0;

  // Returns true if the user is currently logged in as a guest.
  virtual bool IsLoggedInAsGuest() = 0;

  // Invoked when user clicks on button in the launcher and there is no last
  // used window (or CTRL is held with the click).
  virtual void CreateNewWindow() = 0;

  // Invoked when the user clicks on button in the launcher to create a new
  // incognito window.
  virtual void CreateNewIncognitoWindow() = 0;

  // Checks whether the user is allowed to pin apps. Pinning may be disallowed
  // by policy in case there is a pre-defined set of pinned apps.
  virtual bool CanPin() const = 0;

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  virtual void PersistPinnedState() = 0;

  virtual ash::LauncherModel* model() = 0;

  virtual Profile* profile() = 0;

  // Gets the shelf auto-hide behavior on |root_window|.
  virtual ash::ShelfAutoHideBehavior GetShelfAutoHideBehavior(
      aura::RootWindow* root_window) const = 0;

  // Returns |true| if the user is allowed to modify the shelf auto-hide
  // behavior on |root_window|.
  virtual bool CanUserModifyShelfAutoHideBehavior(
      aura::RootWindow* root_window) const = 0;

  // Toggles the shelf auto-hide behavior on |root_window|. Does nothing if the
  // user is not allowed to modify the auto-hide behavior.
  virtual void ToggleShelfAutoHideBehavior(aura::RootWindow* root_window) = 0;

  // The tab no longer represents its previously identified application.
  virtual void RemoveTabFromRunningApp(content::WebContents* tab,
                                       const std::string& app_id) = 0;

  // Notify the controller that the state of an non platform app's tabs
  // have changed,
  virtual void UpdateAppState(content::WebContents* contents,
                              AppState app_state) = 0;

  // Limits application refocusing to urls that match |url| for |id|.
  virtual void SetRefocusURLPatternForTest(ash::LauncherID id,
                                           const GURL& url) = 0;

  // Returns the extension identified by |app_id|.
  virtual const extensions::Extension* GetExtensionForAppID(
      const std::string& app_id) const = 0;

  // ash::LauncherDelegate overrides:
  virtual void OnBrowserShortcutClicked(int event_flags) OVERRIDE = 0;
  virtual void ItemClicked(const ash::LauncherItem& item,
                           const ui::Event& event) OVERRIDE = 0;
  virtual int GetBrowserShortcutResourceId() OVERRIDE = 0;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE = 0;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item, aura::RootWindow* root) OVERRIDE = 0;
  virtual ash::LauncherMenuModel* CreateApplicationMenu(
      const ash::LauncherItem& item,
      int event_flags) OVERRIDE = 0;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE = 0;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE = 0;
  virtual bool ShouldShowTooltip(const ash::LauncherItem& item) OVERRIDE = 0;

  // extensions::AppIconLoader overrides:
  virtual void SetAppImage(const std::string& app_id,
                           const gfx::ImageSkia& image) OVERRIDE = 0;

 protected:
  friend class BrowserLauncherItemControllerTest;
  friend class LauncherPlatformAppBrowserTest;
  friend class LauncherAppBrowserTest;
  // TODO(skuhne): Remove these when the old launcher get removed.
  friend class LauncherPlatformPerAppAppBrowserTest;
  friend class LauncherPerAppAppBrowserTest;

  // Creates a new app shortcut item and controller on the launcher at |index|.
  // Use kInsertItemAtEnd to add a shortcut as the last item.
  virtual ash::LauncherID CreateAppShortcutLauncherItem(
      const std::string& app_id,
      int index) = 0;

  // Sets the AppTabHelper/AppIconLoader, taking ownership of the helper class.
  // These are intended for testing.
  virtual void SetAppTabHelperForTest(AppTabHelper* helper) = 0;
  virtual void SetAppIconLoaderForTest(extensions::AppIconLoader* loader) = 0;
  virtual const std::string& GetAppIdFromLauncherIdForTest(
      ash::LauncherID id) = 0;

 private:
  static ChromeLauncherController* instance_;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
