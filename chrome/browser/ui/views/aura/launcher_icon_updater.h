// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
#pragma once

#include <deque>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"

namespace aura {
class Window;
}
namespace aura_shell {
class LauncherModel;
}

// LauncherIconUpdater is responsible for keeping the launcher representation
// of a window up to date with the tabs.
class LauncherIconUpdater : public TabStripModelObserver {
 public:
  LauncherIconUpdater(TabStripModel* tab_model,
                      aura_shell::LauncherModel* launcher_model,
                      aura::Window* window);
  virtual ~LauncherIconUpdater();

  // TabStripModel overrides:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) OVERRIDE;
  virtual void TabSelectionChanged(
      TabStripModel* tab_strip_model,
      const TabStripSelectionModel& old_model) OVERRIDE;
  virtual void TabChangedAt(
      TabContentsWrapper* tab,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) OVERRIDE;

 private:
  typedef std::deque<TabContentsWrapper*> Tabs;

  // Updates the launcher from the current set of tabs.
  void UpdateLauncher();

  TabStripModel* tab_model_;

  aura_shell::LauncherModel* launcher_model_;

  aura::Window* window_;

  // The tabs. This is an MRU cache of the tabs in the tabstrip model.
  Tabs tabs_;

  DISALLOW_COPY_AND_ASSIGN(LauncherIconUpdater);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
