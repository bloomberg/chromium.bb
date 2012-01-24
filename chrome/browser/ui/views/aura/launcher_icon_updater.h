// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "ash/launcher/launcher_types.h"

class Browser;
class TabContentsWrapper;

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;
}

// LauncherIconUpdater is responsible for keeping the launcher representation
// of a window up to date as the tab strip changes.
class LauncherIconUpdater : public TabStripModelObserver {
 public:
  enum Type {
    TYPE_APP,
    TYPE_TABBED
  };

  // Interface used to load app icons. This is in it's own class so that it can
  // be mocked.
  class AppIconLoader {
   public:
    virtual ~AppIconLoader() {}

    // Returns the app id of the specified tab, or an empty string if there is
    // no app.
    virtual std::string GetAppID(TabContentsWrapper* tab) = 0;

    // Invoked when a tab is removed. The AppIconLoader should cancel loading
    // of images for the specified tab.
    virtual void Remove(TabContentsWrapper* tab) = 0;

    // Fetches the image for the specified tab. When done (which may be
    // synchronous), this should invoke SetAppImage() on the
    // LauncherIconUpdater.
    virtual void FetchImage(TabContentsWrapper* tab) = 0;
  };

  LauncherIconUpdater(aura::Window* window,
                      TabStripModel* tab_model,
                      ash::LauncherModel* launcher_model,
                      Type type);
  virtual ~LauncherIconUpdater();

  // Sets up this LauncherIconUpdater.
  void Init();

  // Creates and returns a new LauncherIconUpdater for |browser|. This returns
  // NULL if a LauncherIconUpdater is not needed for the specified browser.
  static LauncherIconUpdater* Create(Browser* browser);

  // Activates the browser and if necessary tab identified by |id|.
  static void ActivateByID(ash::LauncherID id);

  // Returns the title of the browser and/or tab identified by |id|.
  static string16 GetTitleByID(ash::LauncherID id);

  // Returns the LauncherIconUpdater managing the launcher with the specified
  // id. If |id| corresponds to an app tab, |tab| is set appropriately.
  static const LauncherIconUpdater* GetLauncherByID(
      ash::LauncherID id,
      TabContentsWrapper** tab);

  // Sets the image for an app tab. This is intended to be invoked from the
  // AppIconLoader.
  void SetAppImage(TabContentsWrapper* tab, SkBitmap* image);

  // Sets the AppIconLoader, taking ownership of |loader|. This is intended for
  // testing.
  void SetAppIconLoaderForTest(AppIconLoader* loader);

  // TabStripModel overrides:
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabChangedAt(
      TabContentsWrapper* tab,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground);
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index);
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) OVERRIDE;

 private:
  // AppTabDetails is used to identify a launcher item that corresponds to an
  // app tab.
  struct AppTabDetails {
    AppTabDetails();
    ~AppTabDetails();

    // ID of the launcher item.
    ash::LauncherID id;

    // ID of the app (corresponds to Extension::id()).
    std::string app_id;
  };

  // Used to identify what an update corresponds to.
  enum UpdateType {
    UPDATE_TAB_REMOVED,
    UPDATE_TAB_CHANGED,
    UPDATE_TAB_INSERTED,
  };

  typedef std::map<TabContentsWrapper*, AppTabDetails> AppTabMap;
  typedef std::vector<LauncherIconUpdater*> Instances;

  // Updates the launcher from |tab|.
  void UpdateLauncher(TabContentsWrapper* tab);

  // Invoked when a tab changes in some way. Updates the Launcher appropriately.
  void UpdateAppTabState(TabContentsWrapper* tab, UpdateType update_type);

  // Creates a launcher item for |tab|.
  void AddAppItem(TabContentsWrapper* tab);

  // Returns the LauncherItem that represents an app tab. |id| identifies the
  // launcher id of the item. If -1 the id is assigned from the LauncherModel.
  ash::LauncherItem AppLauncherItem(TabContentsWrapper* tab,
                                    ash::LauncherID id);

  // Creates a tabbed launcher item.
  void CreateTabbedItem();

  // Removes the item from the LauncherModel identified by |id|.
  void RemoveItemByID(ash::LauncherID id);

  // Returns true if this LauncherIconUpdater created the launcher item with the
  // specified id. Returns true if it did. If the id corresponds to an app tab,
  // |tab| is set to the TabContentsWrapper for the app tab.
  bool ContainsID(ash::LauncherID id, TabContentsWrapper** tab);

  // Browser window we're in.
  aura::Window* window_;

  TabStripModel* tab_model_;

  ash::LauncherModel* launcher_model_;

  // Whether this corresponds to an app or tabbed browser.
  const Type type_;

  // This is one of three possible values:
  // . If type_ == TYPE_APP, this is the ID of the app item.
  // . If type_ == TYPE_TABBED and all the tabs are app tabs this is -1.
  // . Otherwise this is the id of the TYPE_TABBED item.
  ash::LauncherID item_id_;

  // Used for any app tabs.
  AppTabMap app_map_;

  // Used to load the image for an app tab.
  scoped_ptr<AppIconLoader> app_icon_loader_;

  // Existing instances.
  static Instances* instances_;

  DISALLOW_COPY_AND_ASSIGN(LauncherIconUpdater);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
