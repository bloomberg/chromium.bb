// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SHOWER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SHOWER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/keep_alive_service.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_factory.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_win.h"
#include "ui/app_list/pagination_model.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace app_list {
class AppListModel;
}

// Creates and shows AppListViewWins as needed. This class is created and
// destroyed with the AppListController, meaning it has a lifetime equivalent to
// Chrome's.
class AppListShower {
 public:
  AppListShower(scoped_ptr<AppListViewFactory> factory,
                scoped_ptr<KeepAliveService> keep_alive);
  ~AppListShower();

  void set_can_close(bool can_close) {
    can_close_app_list_ = can_close;
  }

  void ShowAndReacquireFocus(Profile* requested_profile);
  void ShowForProfile(Profile* requested_profile);
  gfx::NativeWindow GetWindow();

  AppListViewWin* view() {
    return view_.get();
  }

  // Create or recreate, and initialize |view_| from |requested_profile|.
  void CreateViewForProfile(Profile* requested_profile);

  void DismissAppList();
  void CloseAppList();
  bool IsAppListVisible() const;
  void WarmupForProfile(Profile* profile);
  bool HasView() const;

 private:
  scoped_ptr<AppListViewFactory> factory_;
  scoped_ptr<KeepAliveService> keep_alive_service_;
  scoped_ptr<AppListViewWin> view_;
  Profile* profile_;
  bool can_close_app_list_;

  // PaginationModel that is shared across all views.
  app_list::PaginationModel pagination_model_;

  // Used to keep the browser process alive while the app list is visible.

  DISALLOW_COPY_AND_ASSIGN(AppListShower);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SHOWER_H_
