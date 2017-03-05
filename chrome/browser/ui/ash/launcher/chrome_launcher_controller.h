// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ash/public/cpp/shelf_application_menu_item.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/ash/app_launcher_id.h"
#include "chrome/browser/ui/ash/launcher/settings_window_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

class AccountId;
class ArcAppDeferredLauncherController;
class BrowserShortcutLauncherItemController;
class GURL;
class LauncherControllerHelper;
class LauncherItemController;

namespace ash {
class WmShelf;
}

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

namespace ui {
class BaseWindow;
}

// ChromeLauncherController manages the launcher items needed for content
// windows. Launcher items have a type, an optional app id, and a controller.
// Implements mojom::ShelfObserver and is a client of mojom::ShelfController.
class ChromeLauncherController : public ash::mojom::ShelfObserver,
                                 public AppIconLoaderDelegate {
 public:
  // Used to update the state of non plaform apps, as web contents change.
  enum AppState {
    APP_STATE_ACTIVE,
    APP_STATE_WINDOW_ACTIVE,
    APP_STATE_INACTIVE,
    APP_STATE_REMOVED
  };

  // Returns the single ChromeLauncherController instance.
  static ChromeLauncherController* instance() { return instance_; }
  // TODO(crbug.com/654622): Remove this when tests are fixed.
  static void set_instance_for_test(ChromeLauncherController* instance) {
    instance_ = instance;
  }

  Profile* profile() const { return profile_; }

  LauncherControllerHelper* launcher_controller_helper() {
    return launcher_controller_helper_.get();
  }

  ~ChromeLauncherController() override;

  // Initializes this ChromeLauncherController and calls OnInit.
  void Init();

  // Creates a new app item on the shelf for |controller|.
  virtual ash::ShelfID CreateAppLauncherItem(LauncherItemController* controller,
                                             const std::string& app_id,
                                             ash::ShelfItemStatus status) = 0;

  // Returns the shelf item with the given id, or null if |id| isn't found.
  virtual const ash::ShelfItem* GetItem(ash::ShelfID id) const = 0;

  // Updates the type of an item.
  virtual void SetItemType(ash::ShelfID id, ash::ShelfItemType type) = 0;

  // Updates the running status of an item. It will also update the status of
  // browsers shelf item if needed.
  virtual void SetItemStatus(ash::ShelfID id, ash::ShelfItemStatus status) = 0;

  // Updates the controller associated with id (which should be a shortcut).
  // Takes ownership of |controller|.
  // TODO(skuhne): Pass in scoped_ptr to make ownership clear.
  virtual void SetItemController(ash::ShelfID id,
                                 LauncherItemController* controller) = 0;

  // Closes or unpins the shelf item.
  virtual void CloseLauncherItem(ash::ShelfID id) = 0;

  // Pins the specified id. Currently only supports platform apps.
  virtual void Pin(ash::ShelfID id) = 0;

  // Unpins the specified id, closing if not running.
  virtual void Unpin(ash::ShelfID id) = 0;

  // Returns true if the item identified by |id| is pinned.
  virtual bool IsPinned(ash::ShelfID id) = 0;

  // Pins/unpins the specified id.
  virtual void TogglePinned(ash::ShelfID id) = 0;

  // If there is no item in the shelf for application |app_id|, one is created.
  // The (existing or created) shelf items get then locked against a user's
  // un-pinning removal. Used for V1 apps opened as windows that aren't pinned.
  virtual void LockV1AppWithID(const std::string& app_id) = 0;

  // A previously locked shelf item of type |app_id| gets unlocked. If the
  // lock count reaches 0 and the item is not pinned it will go away.
  virtual void UnlockV1AppWithID(const std::string& app_id) = 0;

  // Requests that the shelf item controller specified by |id| open a new
  // instance of the app.  |event_flags| holds the flags of the event which
  // triggered this command.
  virtual void Launch(ash::ShelfID id, int event_flags) = 0;

  // Closes the specified item.
  virtual void Close(ash::ShelfID id) = 0;

  // Returns true if the specified item is open.
  virtual bool IsOpen(ash::ShelfID id) = 0;

  // Returns true if the specified item is for a platform app.
  virtual bool IsPlatformApp(ash::ShelfID id) = 0;

  // Opens a new instance of the application identified by the AppLauncherId.
  // Used by the app-list, and by pinned-app shelf items.
  void LaunchApp(ash::AppLauncherId id,
                 ash::ShelfLaunchSource source,
                 int event_flags);

  // If |app_id| is running, reactivates the app's most recently active window,
  // otherwise launches and activates the app.
  // Used by the app-list, and by pinned-app shelf items.
  virtual void ActivateApp(const std::string& app_id,
                           ash::ShelfLaunchSource source,
                           int event_flags) = 0;

  // Set the image for a specific shelf item (e.g. when set by the app).
  virtual void SetLauncherItemImage(ash::ShelfID shelf_id,
                                    const gfx::ImageSkia& image) = 0;

  // Notify the controller that the state of an non platform app's tabs
  // have changed,
  virtual void UpdateAppState(content::WebContents* contents,
                              AppState app_state) = 0;

  // Returns ShelfID for |contents|. If |contents| is not an app or is not
  // pinned, returns the id of browser shrotcut.
  virtual ash::ShelfID GetShelfIDForWebContents(
      content::WebContents* contents) = 0;

  // Limits application refocusing to urls that match |url| for |id|.
  virtual void SetRefocusURLPatternForTest(ash::ShelfID id,
                                           const GURL& url) = 0;

  // Activates a |window|. If |allow_minimize| is true and the system allows
  // it, the the window will get minimized instead.
  // Returns the action performed. Should be one of SHELF_ACTION_NONE,
  // SHELF_ACTION_WINDOW_ACTIVATED, or SHELF_ACTION_WINDOW_MINIMIZED.
  virtual ash::ShelfAction ActivateWindowOrMinimizeIfActive(
      ui::BaseWindow* window,
      bool allow_minimize) = 0;

  // Called when the active user has changed.
  virtual void ActiveUserChanged(const std::string& user_email) = 0;

  // Called when a user got added to the session.
  virtual void AdditionalUserAddedToSession(Profile* profile) = 0;

  // Get the list of all running incarnations of this item.
  virtual ash::ShelfAppMenuItemList GetAppMenuItemsForTesting(
      const ash::ShelfItem& item) = 0;

  // Get the list of all tabs which belong to a certain application type.
  virtual std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      const std::string& app_id) = 0;

  // Activates a specified shell application by app id and window index.
  virtual void ActivateShellApp(const std::string& app_id,
                                int window_index) = 0;

  // Checks if a given |web_contents| is known to be associated with an
  // application of type |app_id|.
  virtual bool IsWebContentHandledByApplication(
      content::WebContents* web_contents,
      const std::string& app_id) = 0;

  // Check if the gMail app is loaded and it can handle the given web content.
  // This special treatment is required to address crbug.com/234268.
  virtual bool ContentCanBeHandledByGmailApp(
      content::WebContents* web_contents) = 0;

  // Get the favicon for the application list entry for |web_contents|.
  // Note that for incognito windows the incognito icon will be returned.
  // If |web_contents| has not loaded, returns the default favicon.
  virtual gfx::Image GetAppListIcon(
      content::WebContents* web_contents) const = 0;

  // Get the title for the applicatoin list entry for |web_contents|.
  // If |web_contents| has not loaded, returns "Net Tab".
  virtual base::string16 GetAppListTitle(
      content::WebContents* web_contents) const = 0;

  // Returns the LauncherItemController of BrowserShortcut.
  virtual BrowserShortcutLauncherItemController*
  GetBrowserShortcutLauncherItemController() = 0;

  virtual LauncherItemController* GetLauncherItemController(
      const ash::ShelfID id) = 0;

  // Check if the shelf visibility (location, visibility) will change with a new
  // user profile or not. However, since the full visibility calculation of the
  // shelf cannot be performed here, this is only a probability used for
  // animation predictions.
  virtual bool ShelfBoundsChangesProbablyWithUser(
      ash::WmShelf* shelf,
      const AccountId& account_id) const = 0;

  // Called when the user profile is fully loaded and ready to switch to.
  virtual void OnUserProfileReadyToSwitch(Profile* profile) = 0;

  // Controller to launch ARC apps in deferred mode.
  virtual ArcAppDeferredLauncherController* GetArcDeferredLauncher() = 0;

  // Get the launch ID for a given shelf ID.
  virtual const std::string& GetLaunchIDForShelfID(ash::ShelfID id) = 0;

  AppIconLoader* GetAppIconLoaderForApp(const std::string& app_id);

  // Sets the shelf auto-hide and/or alignment behavior from prefs.
  void SetShelfAutoHideBehaviorFromPrefs();
  void SetShelfAlignmentFromPrefs();
  void SetShelfBehaviorsFromPrefs();

  // Sets LauncherControllerHelper or AppIconLoader for test, taking ownership.
  void SetLauncherControllerHelperForTest(
      std::unique_ptr<LauncherControllerHelper> helper);
  void SetAppIconLoadersForTest(
      std::vector<std::unique_ptr<AppIconLoader>>& loaders);

  void SetProfileForTest(Profile* profile);

 protected:
  ChromeLauncherController();

  // Called after Init; allows subclasses to perform additional initialization.
  virtual void OnInit() = 0;

  // Connects or reconnects to the mojom::ShelfController interface in ash.
  // Returns true if connected; virtual for unit tests.
  virtual bool ConnectToShelfController();

  // Accessor for subclasses to interact with the shelf controller.
  ash::mojom::ShelfControllerPtr& shelf_controller() {
    return shelf_controller_;
  }

  // Attach to a specific profile.
  virtual void AttachProfile(Profile* profile_to_attach);

  // ash::mojom::ShelfObserver:
  void OnShelfCreated(int64_t display_id) override;
  void OnAlignmentChanged(ash::ShelfAlignment alignment,
                          int64_t display_id) override;
  void OnAutoHideBehaviorChanged(ash::ShelfAutoHideBehavior auto_hide,
                                 int64_t display_id) override;

 private:
  friend class TestChromeLauncherControllerImpl;

  // AppIconLoaderDelegate:
  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override;

  static ChromeLauncherController* instance_;

  // The currently loaded profile used for prefs and loading extensions. This is
  // NOT necessarily the profile new windows are created with. Note that in
  // multi-profile use cases this might change over time.
  Profile* profile_ = nullptr;

  // Ash's mojom::ShelfController used to change shelf state.
  ash::mojom::ShelfControllerPtr shelf_controller_;

  // The binding this instance uses to implment mojom::ShelfObserver
  mojo::AssociatedBinding<ash::mojom::ShelfObserver> observer_binding_;

  // True when setting a shelf pref in response to an observer notification.
  bool updating_shelf_pref_from_observer_ = false;

  // Used to get app info for tabs.
  std::unique_ptr<LauncherControllerHelper> launcher_controller_helper_;

  // An observer that manages the shelf title and icon for settings windows.
  SettingsWindowObserver settings_window_observer_;

  // Used to load the images for app items.
  std::vector<std::unique_ptr<AppIconLoader>> app_icon_loaders_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
