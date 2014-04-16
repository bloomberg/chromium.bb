// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list.h"
#include "chrome/browser/ui/app_list/app_list_factory.h"
#include "ui/app_list/pagination_model.h"
#include "ui/gfx/native_widget_types.h"

namespace app_list {
class AppListModel;
}

class AppListShowerUnitTest;
class Profile;
class ScopedKeepAlive;

// Creates and shows an AppList as needed for non-Ash desktops. It is owned
// by AppListService.
class AppListShower {
 public:
  AppListShower(scoped_ptr<AppListFactory> factory,
                AppListService* service);
  ~AppListShower();

  void set_can_close(bool can_close) {
    can_close_app_list_ = can_close;
  }

  void ShowForProfile(Profile* requested_profile);
  gfx::NativeWindow GetWindow();

  AppList* app_list() { return app_list_.get(); }
  Profile* profile() const { return profile_; }

  // Create or recreate, and initialize |app_list_| from |requested_profile|.
  void CreateViewForProfile(Profile* requested_profile);

  void DismissAppList();
  void CloseAppList();
  bool IsAppListVisible() const;
  void WarmupForProfile(Profile* profile);
  bool HasView() const;

 private:
  friend class ::AppListShowerUnitTest;

  void ResetKeepAlive();

  scoped_ptr<AppListFactory> factory_;
  scoped_ptr<ScopedKeepAlive> keep_alive_;
  scoped_ptr<AppList> app_list_;
  AppListService* service_;  // Weak ptr, owns this.
  Profile* profile_;
  bool can_close_app_list_;

  // Used to keep the browser process alive while the app list is visible.

  DISALLOW_COPY_AND_ASSIGN(AppListShower);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_H_
