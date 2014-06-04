// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_VIEWS_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace app_list {
class AppListView;
}

class AppListShowerDelegate;
class AppListShowerUnitTest;
class Profile;
class ScopedKeepAlive;

// Creates and shows an AppList as needed for non-Ash desktops. It is owned by
// AppListServiceViews.
class AppListShower {
 public:
  explicit AppListShower(AppListShowerDelegate* delegate);
  virtual ~AppListShower();

  void ShowForProfile(Profile* requested_profile);
  gfx::NativeWindow GetWindow();

  app_list::AppListView* app_list() { return app_list_; }
  Profile* profile() const { return profile_; }

  // Create or recreate, and initialize |app_list_| from |requested_profile|.
  void CreateViewForProfile(Profile* requested_profile);

  void DismissAppList();

  // Virtual functions mocked out in tests.
  virtual void HandleViewBeingDestroyed();
  virtual bool IsAppListVisible() const;
  void WarmupForProfile(Profile* profile);
  virtual bool HasView() const;

 protected:
  virtual app_list::AppListView* MakeViewForCurrentProfile();
  virtual void UpdateViewForNewProfile();

  // Shows the app list, activates it, and ensures the taskbar icon is updated.
  virtual void Show();
  virtual void Hide();

 private:
  friend class ::AppListShowerUnitTest;

  void ResetKeepAlive();

  AppListShowerDelegate* delegate_;  // Weak. Owns this.

  // The profile currently shown by |app_list_|.
  Profile* profile_;

  // The view, once created. Owned by native widget.
  app_list::AppListView* app_list_;

  // Used to keep the browser process alive while the app list is visible.
  scoped_ptr<ScopedKeepAlive> keep_alive_;

  bool window_icon_updated_;

  DISALLOW_COPY_AND_ASSIGN(AppListShower);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_VIEWS_H_
