// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_UPDATER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_UPDATER_H_
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
class ChromeLauncherDelegate;
class TabContentsWrapper;

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;
}

// LauncherUpdater is responsible for keeping the launcher representation of a
// window up to date as the tab strip changes.
class LauncherUpdater : public TabStripModelObserver {
 public:
  enum Type {
    TYPE_APP,
    TYPE_PANEL,
    TYPE_TABBED
  };

  LauncherUpdater(aura::Window* window,
                  TabStripModel* tab_model,
                  ChromeLauncherDelegate* launcher_delegate,
                  Type type,
                  const std::string& app_id);
  virtual ~LauncherUpdater();

  // Sets up this LauncherUpdater.
  void Init();

  // Creates and returns a new LauncherUpdater for |browser|. This returns
  // NULL if a LauncherUpdater is not needed for the specified browser.
  static LauncherUpdater* Create(Browser* browser);

  aura::Window* window() { return window_; }

  TabStripModel* tab_model() { return tab_model_; }

  Type type() const { return type_; }

  TabContentsWrapper* GetTab(ash::LauncherID id);

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
                             bool foreground) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) OVERRIDE;
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

  // Updates the launcher from |tab|.
  void UpdateLauncher(TabContentsWrapper* tab);

  // Invoked when a tab changes in some way. Updates the Launcher appropriately.
  void UpdateAppTabState(TabContentsWrapper* tab, UpdateType update_type);

  // Creates a launcher item for |tab|.
  void AddAppItem(TabContentsWrapper* tab);

  void RegisterAppItem(ash::LauncherID id, TabContentsWrapper* tab);

  // Creates a tabbed launcher item.
  void CreateTabbedItem();

  // Returns true if this LauncherUpdater created the launcher item with the
  // specified id. Returns true if it did. If the id corresponds to an app tab,
  // |tab| is set to the TabContentsWrapper for the app tab.
  bool ContainsID(ash::LauncherID id, TabContentsWrapper** tab);

  ash::LauncherModel* launcher_model();

  // Browser window we're in.
  aura::Window* window_;

  TabStripModel* tab_model_;

  ChromeLauncherDelegate* launcher_delegate_;

  // Whether this corresponds to an app or tabbed browser.
  const Type type_;

  const std::string app_id_;

  // This is one of three possible values:
  // . If type_ == TYPE_APP, this is the ID of the app item.
  // . If type_ == TYPE_TABBED and all the tabs are app tabs this is -1.
  // . Otherwise this is the id of the TYPE_TABBED item.
  ash::LauncherID item_id_;

  // Used for any app tabs.
  AppTabMap app_map_;

  DISALLOW_COPY_AND_ASSIGN(LauncherUpdater);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_UPDATER_H_
