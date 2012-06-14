// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#pragma once

#include <list>
#include <map>
#include <string>

#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;

namespace client {
class ActivationClient;
}

}

class BrowserLauncherItemController;
class BrowserLauncherItemControllerTest;
class PrefService;
class Profile;
class TabContents;

// ChromeLauncherController manages the launcher items needed for tabbed
// browsers (BrowserLauncherItemController) and browser shortcuts.
class ChromeLauncherController : public ash::LauncherDelegate,
                                 public ash::LauncherModelObserver,
                                 public content::NotificationObserver,
                                 public ShellWindowRegistry::Observer,
                                 public aura::client::ActivationChangeObserver,
                                 public aura::WindowObserver {
 public:
  // Indicates if a launcher item is incognito or not.
  enum IncognitoState {
    STATE_INCOGNITO,
    STATE_NOT_INCOGNITO,
  };

  // Interface used to load app icons. This is in it's own class so that it can
  // be mocked.
  class AppIconLoader {
   public:
    virtual ~AppIconLoader() {}

    // Returns the app id of the specified tab, or an empty string if there is
    // no app.
    virtual std::string GetAppID(TabContents* tab) = 0;

    // Returns true if |id| is valid. Used during restore to ignore no longer
    // valid extensions.
    virtual bool IsValidID(const std::string& id) = 0;

    // Fetches the image for the specified id. When done (which may be
    // synchronous), this should invoke SetAppImage() on the LauncherUpdater.
    virtual void FetchImage(const std::string& id) = 0;
  };

  ChromeLauncherController(Profile* profile, ash::LauncherModel* model);
  virtual ~ChromeLauncherController();

  // Initializes this ChromeLauncherController.
  void Init();

  // Returns the single ChromeLauncherController instnace.
  static ChromeLauncherController* instance() { return instance_; }

  // Creates a new tabbed item on the launcher for |controller|.
  ash::LauncherID CreateTabbedLauncherItem(
      BrowserLauncherItemController* controller,
      IncognitoState is_incognito,
      ash::LauncherItemStatus status);

  // Creates a new app item on the launcher for |controller|.
  ash::LauncherID CreateAppLauncherItem(
      BrowserLauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status);

  // Updates the running status of an item.
  void SetItemStatus(ash::LauncherID id, ash::LauncherItemStatus status);

  // Invoked when the underlying browser/app is closed.
  void LauncherItemClosed(ash::LauncherID id);

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

  // Opens the specified item.  |event_flags| holds the flags of the
  // event which triggered this command.
  void Open(ash::LauncherID id, int event_flags);

  // Closes the specified item.
  void Close(ash::LauncherID id);

  // Returns true if the specified item is open.
  bool IsOpen(ash::LauncherID id);

  // Returns the launch type of app for the specified id.
  ExtensionPrefs::LaunchType GetLaunchType(ash::LauncherID id);

  // Returns the id of the app for the specified tab.
  std::string GetAppID(TabContents* tab);

  // Sets the image for an app tab. This is intended to be invoked from the
  // AppIconLoader.
  void SetAppImage(const std::string& app_id, const SkBitmap* image);

  // Returns true if a pinned launcher item with given |app_id| could be found.
  bool IsAppPinned(const std::string& app_id);

  // Pins an app with |app_id| to launcher. If there is a running instance in
  // launcher, the running instance is pinned. If there is no running instance,
  // a new launcher item is created and pinned.
  void PinAppWithID(const std::string& app_id);

  // Updates the launche type of the app for the specified id to |launch_type|.
  void SetLaunchType(ash::LauncherID id,
                     ExtensionPrefs::LaunchType launch_type);

  // Unpins any app items whose id is |app_id|.
  void UnpinAppsWithID(const std::string& app_id);

  // Returns true if the user is currently logged in as a guest.
  bool IsLoggedInAsGuest();

  // Invoked when the user clicks on button in the launcher to create a new
  // incognito window.
  void CreateNewIncognitoWindow();

  // Checks whether the user is allowed to pin apps. Pinning may be disallowed
  // by policy in case there is a pre-defined set of pinned apps.
  bool CanPin() const;

  ash::LauncherModel* model() { return model_; }

  Profile* profile() { return profile_; }

  void SetAutoHideBehavior(ash::ShelfAutoHideBehavior behavior);

  // ash::LauncherDelegate overrides:
  virtual void CreateNewTab() OVERRIDE;
  virtual void CreateNewWindow() OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item,
                           int event_flags) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenuForLauncher() OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE;

  // ash::LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;
  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ShellWindowRegistry::Observer:
  virtual void OnShellWindowAdded(ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowRemoved(ShellWindow* shell_window) OVERRIDE;

  // Overriden from client::ActivationChangeObserver:
  virtual void OnWindowActivated(
      aura::Window* active,
      aura::Window* old_active) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE;

 private:
  friend class BrowserLauncherItemControllerTest;
  friend class ChromeLauncherControllerTest;

  enum ItemType {
    TYPE_APP,
    TYPE_TABBED_BROWSER
  };

  // Used to identity an item on the launcher.
  struct Item {
    Item();
    ~Item();

    // Type of item.
    ItemType item_type;

    // ID of the app.
    std::string app_id;

    // The BrowserLauncherItemController this item came from. NULL if a
    // shortcut.
    BrowserLauncherItemController* controller;
  };

  typedef std::map<ash::LauncherID, Item> IDToItemMap;
  typedef std::map<aura::Window*, ash::LauncherID> WindowToIDMap;
  typedef std::list<aura::Window*> WindowList;

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  void PersistPinnedState();

  // Sets the AppIconLoader, taking ownership of |loader|. This is intended for
  // testing.
  void SetAppIconLoaderForTest(AppIconLoader* loader);

  // Returns the profile used for new windows.
  Profile* GetProfileForNewWindows();

  // Returns item status for given |id|.
  ash::LauncherItemStatus GetItemStatus(ash::LauncherID id) const;

  // Finds the launcher item that represents given |app_id| and updates the
  // pending state.
  void MarkAppPending(const std::string& app_id);

  // Internal helpers for pinning and unpinning that handle both
  // client-triggered and internal pinning operations.
  void DoPinAppWithID(const std::string& app_id);
  void DoUnpinAppsWithID(const std::string& app_id);

  // Re-syncs launcher model with prefs::kPinnedLauncherApps.
  void UpdateAppLaunchersFromPref();

  // Creates an app launcher to insert at |index|. Note that |index| may be
  // adjusted by the model to meet ordering constraints.
  ash::LauncherID InsertAppLauncherItem(
      BrowserLauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status,
      int index);

  static ChromeLauncherController* instance_;

  ash::LauncherModel* model_;

  // Profile used for prefs and loading extensions. This is NOT necessarily the
  // profile new windows are created with.
  Profile* profile_;

  IDToItemMap id_to_item_map_;

  // Allows us to get from an aura::Window to the id of a launcher item.
  // Currently only used for platform app windows.
  WindowToIDMap window_to_id_map_;
  // Maintains the activation order. The first element is most recent.
  // Currently only used for platform app windows.
  WindowList activation_order_;

  // Used to load the image for an app tab.
  scoped_ptr<AppIconLoader> app_icon_loader_;

  content::NotificationRegistrar notification_registrar_;

  PrefChangeRegistrar pref_change_registrar_;
  aura::client::ActivationClient* activation_client_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
