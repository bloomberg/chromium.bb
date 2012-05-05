// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
#pragma once

#include <map>
#include <deque>
#include <string>

#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace ash {
class LauncherModel;
}

class BrowserLauncherItemController;
class BrowserLauncherItemControllerTest;
class PrefService;
class Profile;
class TabContentsWrapper;

// ChromeLauncherController manages the launcher items needed for tabbed
// browsers (BrowserLauncherItemController) and browser shortcuts.
class ChromeLauncherController : public ash::LauncherDelegate,
                                 public ash::LauncherModelObserver,
                                 public content::NotificationObserver {
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
    virtual std::string GetAppID(TabContentsWrapper* tab) = 0;

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

  // Registers the prefs used by ChromeLauncherController.
  static void RegisterUserPrefs(PrefService* user_prefs);

  // Creates a new tabbed item on the launcher for |updater|.
  ash::LauncherID CreateTabbedLauncherItem(
      BrowserLauncherItemController* controller,
      IncognitoState is_incognito,
      ash::LauncherItemStatus status);

  // Creates a new app item on the launcher for |updater|. If there is an
  // existing pinned app that isn't running on the launcher, its id is returned.
  ash::LauncherID CreateAppLauncherItem(
      BrowserLauncherItemController* controller,
      const std::string& app_id,
      ash::LauncherItemStatus status);

  // Updates the running status of an item.
  void SetItemStatus(ash::LauncherID id, ash::LauncherItemStatus status);

  // Invoked when the underlying browser/app is closed.
  void LauncherItemClosed(ash::LauncherID id);

  // Unpins the specified id, closing if not running.
  void Unpin(ash::LauncherID id);

  // Returns true if the item identified by |id| is pinned.
  bool IsPinned(ash::LauncherID id);

  // Pins/unpins the specified id.
  void TogglePinned(ash::LauncherID id);

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  bool IsPinnable(ash::LauncherID id) const;

  // Opens the specified item.
  void Open(ash::LauncherID id);

  // Closes the specified item.
  void Close(ash::LauncherID id);

  // Returns true if the specified item is open.
  bool IsOpen(ash::LauncherID id);

  // Returns the launch type of app for the specified id.
  ExtensionPrefs::LaunchType GetLaunchType(ash::LauncherID id);

  // Returns the id of the app for the specified tab.
  std::string GetAppID(TabContentsWrapper* tab);

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

  ash::LauncherModel* model() { return model_; }

  Profile* profile() { return profile_; }

  void SetAutoHideBehavior(ash::ShelfAutoHideBehavior behavior);

  // ash::LauncherDelegate overrides:
  virtual void CreateNewTab() OVERRIDE;
  virtual void CreateNewWindow() OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenuForLauncher() OVERRIDE;
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE;

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
 private:
  friend class BrowserLauncherItemControllerTest;

  enum ItemType {
    TYPE_APP,
    TYPE_TABBED_BROWSER
  };

  // Used to identity an item on the launcher.
  struct Item {
    Item();
    ~Item();

    bool is_pinned() const { return controller == NULL; }

    // Type of item.
    ItemType item_type;

    // ID of the app.
    std::string app_id;

    // The BrowserLauncherItemController this item came from. NULL if a
    // shortcut.
    BrowserLauncherItemController* controller;
  };

  typedef std::map<ash::LauncherID, Item> IDToItemMap;

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app.
  void PersistPinnedState();

  // Sets the AppIconLoader, taking ownership of |loader|. This is intended for
  // testing.
  void SetAppIconLoaderForTest(AppIconLoader* loader);

  // Returns the profile used for new windows.
  Profile* GetProfileForNewWindows();

  // Checks |pending_pinnned_apps_| list and creates pinned app items for apps
  // that are ready. To maintain the order, the list is iterated from the
  // beginning to the end and stops the iteration when hitting a not-ready app.
  void ProcessPendingPinnedApps();

  static ChromeLauncherController* instance_;

  ash::LauncherModel* model_;

  // Profile used for prefs and loading extensions. This is NOT necessarily the
  // profile new windows are created with.
  Profile* profile_;

  IDToItemMap id_to_item_map_;

  // Used to load the image for an app tab.
  scoped_ptr<AppIconLoader> app_icon_loader_;

  // A list of items that are in pinned app list but corresponding apps are
  // not ready. Keep them in this list and create pinned item when the apps
  // are installed (via sync or external extension provider.) The order of the
  // list reflects the original order in pinned app list.
  std::deque<Item> pending_pinned_apps_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_H_
