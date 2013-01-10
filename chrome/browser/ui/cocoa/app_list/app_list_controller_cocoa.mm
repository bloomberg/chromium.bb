// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "ui/app_list/cocoa/app_list_view_window.h"

namespace {

// The AppListController class manages global resources needed for the app
// list to operate, and controls when the app list is opened and closed.
class AppListController {
 public:
  AppListController() {}
  ~AppListController() {}

  void CreateAppList();
  void ShowAppList();
  void DismissAppList();
 private:
  scoped_nsobject<AppListViewWindow> current_window_;
  base::OneShotTimer<AppListController> timer_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

base::LazyInstance<AppListController>::Leaky g_app_list_controller =
    LAZY_INSTANCE_INITIALIZER;

void AppListController::CreateAppList() {
  current_window_.reset([[AppListViewWindow alloc] initAsBubble]);
}

void AppListController::ShowAppList() {
  const int kLifetimeIntervalMS = 1000;

  if (!current_window_)
    CreateAppList();

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kLifetimeIntervalMS), this,
               &AppListController::DismissAppList);
  [current_window_ makeKeyAndOrderFront:nil];
}

void AppListController::DismissAppList() {
  if (!current_window_)
    return;

  timer_.Stop();
  [current_window_ close];
}

}  // namespace


namespace chrome {

void InitAppList() {
  // TODO(tapted): AppList warmup code coes here.
}

void ShowAppList() {
  // Create the App list.
  g_app_list_controller.Get().ShowAppList();
}

}  // namespace chrome
