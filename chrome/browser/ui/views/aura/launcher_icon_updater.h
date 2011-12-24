// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"

class TabContentsWrapper;

namespace aura {
class Window;
}
namespace ash {
class LauncherModel;
}

// LauncherIconUpdater is responsible for keeping the launcher representation
// of a window up to date as the tab strip changes.
class LauncherIconUpdater : public TabStripModelObserver {
 public:
  LauncherIconUpdater(TabStripModel* tab_model,
                      ash::LauncherModel* launcher_model,
                      aura::Window* window);
  virtual ~LauncherIconUpdater();

  // TabStripModel overrides:
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabChangedAt(
      TabContentsWrapper* tab,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;

 private:
  // Updates the launcher from |tab|.
  void UpdateLauncher(TabContentsWrapper* tab);

  TabStripModel* tab_model_;

  ash::LauncherModel* launcher_model_;

  // Used to index into the model.
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(LauncherIconUpdater);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_ICON_UPDATER_H_
