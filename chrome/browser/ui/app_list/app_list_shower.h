// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list.h"
#include "chrome/browser/ui/app_list/app_list_factory.h"
#include "chrome/browser/ui/app_list/keep_alive_service.h"
#include "ui/app_list/pagination_model.h"
#include "ui/gfx/native_widget_types.h"

namespace app_list {
class AppListModel;
}

namespace content {
class BrowserContext;
}

// Creates and shows AppLists as needed. This class is created and destroyed
// with the AppListController, meaning it has a lifetime equivalent to Chrome's.
class AppListShower {
 public:
  AppListShower(scoped_ptr<AppListFactory> factory,
                scoped_ptr<KeepAliveService> keep_alive);
  ~AppListShower();

  void set_can_close(bool can_close) {
    can_close_app_list_ = can_close;
  }

  void ShowAndReacquireFocus(content::BrowserContext* requested_context);
  void ShowForBrowserContext(content::BrowserContext* requested_context);
  gfx::NativeWindow GetWindow();

  AppList* app_list() {
    return app_list_.get();
  }

  content::BrowserContext* browser_context() const { return browser_context_; }

  // Create or recreate, and initialize |app_list_| from |requested_context|.
  void CreateViewForBrowserContext(content::BrowserContext* requested_context);

  void DismissAppList();
  void CloseAppList();
  bool IsAppListVisible() const;
  void WarmupForProfile(content::BrowserContext* context);
  bool HasView() const;

 private:
  scoped_ptr<AppListFactory> factory_;
  scoped_ptr<KeepAliveService> keep_alive_service_;
  scoped_ptr<AppList> app_list_;
  content::BrowserContext* browser_context_;
  bool can_close_app_list_;

  // Used to keep the browser process alive while the app list is visible.

  DISALLOW_COPY_AND_ASSIGN(AppListShower);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_H_
