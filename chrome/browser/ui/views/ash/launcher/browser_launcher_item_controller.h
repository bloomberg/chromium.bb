// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_BROWSER_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_BROWSER_LAUNCHER_ITEM_CONTROLLER_H_
#pragma once

#include <string>

#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_favicon_loader.h"

class Browser;
class ChromeLauncherController;
class LauncherFaviconLoader;
class TabContentsWrapper;

namespace ash {
class LauncherModel;
}

namespace aura {
class Window;
}

// BrowserLauncherItemController is responsible for keeping the launcher
// representation of a window up to date as the active tab changes.
class BrowserLauncherItemController : public TabStripModelObserver,
                                      public LauncherFaviconLoader::Delegate {
 public:
  // This API is to be used as part of testing only.
  class TestApi {
   public:
    explicit TestApi(BrowserLauncherItemController* controller)
        : controller_(controller) {}
    ~TestApi() {}

    // Returns the launcher id for the browser window.
    ash::LauncherID item_id() const { return controller_->item_id_; }

   private:
    BrowserLauncherItemController* controller_;
  };

  enum Type {
    TYPE_APP_PANEL,
    TYPE_EXTENSION_PANEL,
    TYPE_TABBED
  };

  BrowserLauncherItemController(aura::Window* window,
                  TabStripModel* tab_model,
                  ChromeLauncherController* launcher_controller,
                  Type type,
                  const std::string& app_id);
  virtual ~BrowserLauncherItemController();

  // Sets up this BrowserLauncherItemController.
  void Init();

  // Creates and returns a new BrowserLauncherItemController for |browser|. This
  // returns NULL if a BrowserLauncherItemController is not needed for the
  // specified browser.
  static BrowserLauncherItemController* Create(Browser* browser);

  aura::Window* window() { return window_; }

  TabStripModel* tab_model() { return tab_model_; }

  Type type() const { return type_; }

  LauncherFaviconLoader* favicon_loader() const {
    return favicon_loader_.get();
  }

  // Call to indicate that the window the tabcontents are in has changed its
  // activation state.
  void BrowserActivationStateChanged();

  // TabStripModel overrides:
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabChangedAt(
      TabContentsWrapper* tab,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;

  // LauncherFaviconLoader::Delegate overrides:
  virtual void FaviconUpdated() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserLauncherItemControllerTest, PanelItem);

  // Used to identify what an update corresponds to.
  enum UpdateType {
    UPDATE_TAB_REMOVED,
    UPDATE_TAB_CHANGED,
    UPDATE_TAB_INSERTED,
  };

  // Updates the launcher from |tab|.
  void UpdateLauncher(TabContentsWrapper* tab);

  ash::LauncherModel* launcher_model();

  // Browser window we're in.
  aura::Window* window_;

  TabStripModel* tab_model_;

  ChromeLauncherController* launcher_controller_;

  // Whether this corresponds to an app or tabbed browser.
  const Type type_;

  const std::string app_id_;

  // Whether this is associated with an incognito profile.
  const bool is_incognito_;

  // ID of the item.
  ash::LauncherID item_id_;

  // Loads launcher sized favicons for panels.
  scoped_ptr<LauncherFaviconLoader> favicon_loader_;

  DISALLOW_COPY_AND_ASSIGN(BrowserLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_BROWSER_LAUNCHER_ITEM_CONTROLLER_H_
