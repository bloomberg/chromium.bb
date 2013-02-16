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
#include "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"

namespace gfx {
class ImageSkia;
}

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
  scoped_nsobject<AppListWindowController> window_controller_;
  base::OneShotTimer<AppListController> timer_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

base::LazyInstance<AppListController>::Leaky g_app_list_controller =
    LAZY_INSTANCE_INITIALIZER;

void AppListController::CreateAppList() {
  // TODO(tapted): Create our own AppListViewDelegate subtype, and use it here.
  scoped_ptr<app_list::AppListViewDelegate> delegate;
  scoped_nsobject<AppsGridController> content(
      [[AppsGridController alloc] initWithViewDelegate:delegate.Pass()]);
  window_controller_.reset(
      [[AppListWindowController alloc] initWithGridController:content]);
}

void AppListController::ShowAppList() {
  const int kLifetimeIntervalMS = 1000;

  if (!window_controller_)
    CreateAppList();

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kLifetimeIntervalMS), this,
               &AppListController::DismissAppList);
  [[window_controller_ window] makeKeyAndOrderFront:nil];
}

void AppListController::DismissAppList() {
  if (!window_controller_)
    return;

  timer_.Stop();
  [[window_controller_ window] close];
}

}  // namespace


namespace chrome {

void InitAppList(Profile* profile) {
  // TODO(tapted): AppList warmup code coes here.
}

void ShowAppList(Profile* profile) {
  g_app_list_controller.Get().ShowAppList();
}

void NotifyAppListOfBeginExtensionInstall(
    Profile* profile,
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon) {
}

void NotifyAppListOfDownloadProgress(
    Profile* profile,
    const std::string& extension_id,
    int percent_downloaded) {
}

void NotifyAppListOfExtensionInstallFailure(
    Profile* profile,
    const std::string& extension_id) {
}

}  // namespace chrome
