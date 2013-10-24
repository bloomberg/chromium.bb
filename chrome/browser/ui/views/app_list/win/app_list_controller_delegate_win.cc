// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"

#include "chrome/browser/ui/app_list/app_list_icon_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_service_win.h"
#include "ui/base/resource/resource_bundle.h"

AppListControllerDelegateWin::AppListControllerDelegateWin(
    AppListServiceWin* service)
    : AppListControllerDelegateImpl(service),
      service_(service) {}

AppListControllerDelegateWin::~AppListControllerDelegateWin() {}

void AppListControllerDelegateWin::ViewClosing() {
  service_->OnAppListClosing();
}

gfx::ImageSkia AppListControllerDelegateWin::GetWindowIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(GetAppListIconResourceId());
  return *resource;
}

void AppListControllerDelegateWin::OnShowExtensionPrompt() {
  service_->set_can_close(false);
}

void AppListControllerDelegateWin::OnCloseExtensionPrompt() {
  service_->set_can_close(true);
}

bool AppListControllerDelegateWin::CanDoCreateShortcutsFlow() {
  return true;
}
