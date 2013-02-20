// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_LAUNCHER_ITEM_CONTROLLER_H_

#include <string>

#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/ash/launcher/launcher_favicon_loader.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/aura/window_observer.h"

class Browser;
class LauncherFaviconLoader;

namespace ash {
class LauncherModel;
}

// BrowserLauncherItemController is responsible for keeping the launcher
// representation of a window up to date as the active tab changes.
class BrowserLauncherItemController : public LauncherItemController,
                                      public TabStripModelObserver,
                                      public LauncherFaviconLoader::Delegate,
                                      public aura::WindowObserver {
 public:
  // This API is to be used as part of testing only.
  class TestApi {
   public:
    explicit TestApi(BrowserLauncherItemController* controller)
        : controller_(controller) {}
    ~TestApi() {}

    // Returns the launcher id for the browser window.
    ash::LauncherID item_id() const { return controller_->launcher_id(); }

   private:
    BrowserLauncherItemController* controller_;
  };

  BrowserLauncherItemController(Type type,
                                aura::Window* window,
                                TabStripModel* tab_model,
                                ChromeLauncherController* launcher_controller,
                                const std::string& app_id);
  virtual ~BrowserLauncherItemController();

  // Sets up this BrowserLauncherItemController.
  void Init();

  // Creates and returns a new BrowserLauncherItemController for |browser|. This
  // returns NULL if a BrowserLauncherItemController is not needed for the
  // specified browser.
  static BrowserLauncherItemController* Create(Browser* browser);

  LauncherFaviconLoader* favicon_loader() const {
    return favicon_loader_.get();
  }

  // Call to indicate that the window the tabcontents are in has changed its
  // activation state.
  void BrowserActivationStateChanged();

  // LauncherItemController overrides:
  virtual string16 GetTitle() OVERRIDE;
  virtual bool HasWindow(aura::Window* window) const OVERRIDE;
  virtual bool IsOpen() const OVERRIDE;
  virtual void Launch(int event_flags) OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Clicked(const ui::Event& event) OVERRIDE;
  virtual void OnRemoved() OVERRIDE;
  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE;
  virtual ChromeLauncherAppMenuItems GetApplicationList() OVERRIDE;

  // TabStripModel overrides:
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void TabChangedAt(
      content::WebContents* contents,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;

  // LauncherFaviconLoader::Delegate overrides:
  virtual void FaviconUpdated() OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserLauncherItemControllerTest, PanelItem);

  // Used to identify what an update corresponds to.
  enum UpdateType {
    UPDATE_TAB_REMOVED,
    UPDATE_TAB_CHANGED,
    UPDATE_TAB_INSERTED,
  };

  // Updates the launcher item status base on the activation and attention
  // state of the window.
  void UpdateItemStatus();

  // Updates the launcher from |tab|.
  void UpdateLauncher(content::WebContents* tab);

  void UpdateAppState(content::WebContents* tab);

  ash::LauncherModel* launcher_model();

  // Browser window we're in.
  aura::Window* window_;

  TabStripModel* tab_model_;

  // Whether this is associated with an incognito profile.
  const bool is_incognito_;

  // Loads launcher sized favicons for panels.
  scoped_ptr<LauncherFaviconLoader> favicon_loader_;

  DISALLOW_COPY_AND_ASSIGN(BrowserLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_BROWSER_LAUNCHER_ITEM_CONTROLLER_H_
