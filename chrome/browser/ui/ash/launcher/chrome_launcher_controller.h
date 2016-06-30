// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "extensions/common/constants.h"

class ArcAppDeferredLauncherController;
class Browser;
class BrowserShortcutLauncherItemController;
class GURL;
class LauncherItemController;

namespace ash {
class Shelf;
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

// A list of the elements which makes up a simple menu description.
typedef ScopedVector<ChromeLauncherAppMenuItem> ChromeLauncherAppMenuItems;

// ChromeLauncherController manages the launcher items needed for content
// windows. Launcher items have a type, an optional app id, and a controller.
// There are different kinds of launcher item controllers, all extending the
// LauncherItemController class.
class ChromeLauncherController {
 public:
  // Used to update the state of non plaform apps, as web contents change.
  enum AppState {
    APP_STATE_ACTIVE,
    APP_STATE_WINDOW_ACTIVE,
    APP_STATE_INACTIVE,
    APP_STATE_REMOVED
  };

  // Set the ChromeLauncherController singleton.
  static void set_instance(ChromeLauncherController* instance) {
    instance_ = instance;
  }

  // Returns the single ChromeLauncherController instance.
  static ChromeLauncherController* instance() { return instance_; }

  virtual ~ChromeLauncherController();

  // Initializes this ChromeLauncherController.
  virtual void Init() = 0;

  // Creates a new app item on the shelf for |controller|.
  virtual ash::ShelfID CreateAppLauncherItem(LauncherItemController* controller,
                                             const std::string& app_id,
                                             ash::ShelfItemStatus status) = 0;

  // Updates the running status of an item. It will also update the status of
  // browsers shelf item if needed.
  virtual void SetItemStatus(ash::ShelfID id, ash::ShelfItemStatus status) = 0;

  // Updates the controller associated with id (which should be a shortcut).
  // |controller| will be owned by the |ChromeLauncherController| and then
  // passed on to |ShelfItemDelegateManager|.
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

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  virtual bool IsPinnable(ash::ShelfID id) const = 0;

  // If there is no shelf item in the shelf for application |app_id|, one
  // gets created. The (existing or created) shelf items get then locked
  // against a users un-pinning removal.
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

  // Opens a new instance of the application identified by |app_id|.
  // Used by the app-list, and by pinned-app shelf items.
  virtual void LaunchApp(const std::string& app_id,
                         ash::LaunchSource source,
                         int event_flags) = 0;

  // If |app_id| is running, reactivates the app's most recently active window,
  // otherwise launches and activates the app.
  // Used by the app-list, and by pinned-app shelf items.
  virtual void ActivateApp(const std::string& app_id,
                           ash::LaunchSource source,
                           int event_flags) = 0;

  // Returns the launch type of app for the specified id.
  virtual extensions::LaunchType GetLaunchType(ash::ShelfID id) = 0;

  // Set the image for a specific shelf item (e.g. when set by the app).
  virtual void SetLauncherItemImage(ash::ShelfID shelf_id,
                                    const gfx::ImageSkia& image) = 0;

  // Find out if the given application |id| is a windowed app item and not a
  // pinned item in the shelf.
  virtual bool IsWindowedAppInLauncher(const std::string& app_id) = 0;

  // Updates the launch type of the app for the specified id to |launch_type|.
  virtual void SetLaunchType(ash::ShelfID id,
                             extensions::LaunchType launch_type) = 0;

  // Accessor to the currently loaded profile. Note that in multi profile use
  // cases this might change over time.
  virtual Profile* GetProfile() = 0;

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
  // Returns the action performed. Should be one of kNoAction,
  // kExistingWindowActivated, or kExistingWindowMinimized.
  virtual ash::ShelfItemDelegate::PerformedAction
  ActivateWindowOrMinimizeIfActive(ui::BaseWindow* window,
                                   bool allow_minimize) = 0;

  // Called when the active user has changed.
  virtual void ActiveUserChanged(const std::string& user_email) = 0;

  // Called when a user got added to the session.
  virtual void AdditionalUserAddedToSession(Profile* profile) = 0;

  // Get the list of all running incarnations of this item.
  // |event_flags| specifies the flags which were set by the event which
  // triggered this menu generation. It can be used to generate different lists.
  virtual ChromeLauncherAppMenuItems GetApplicationList(
      const ash::ShelfItem& item,
      int event_flags) = 0;

  // Get the list of all tabs which belong to a certain application type.
  virtual std::vector<content::WebContents*> GetV1ApplicationsFromAppId(
      const std::string& app_id) = 0;

  // Activates a specified shell application.
  virtual void ActivateShellApp(const std::string& app_id, int index) = 0;

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
      ash::Shelf* shelf,
      const std::string& user_id) const = 0;

  // Called when the user profile is fully loaded and ready to switch to.
  virtual void OnUserProfileReadyToSwitch(Profile* profile) = 0;

  // Controller to launch Arc apps in deferred mode.
  virtual ArcAppDeferredLauncherController* GetArcDeferredLauncher() = 0;

 protected:
  ChromeLauncherController();

 private:
  static ChromeLauncherController* instance_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
