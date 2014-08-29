// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"

#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_icon_win.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/base/resource/resource_bundle.h"

AppListControllerDelegateWin::AppListControllerDelegateWin(
    AppListServiceViews* service)
    : AppListControllerDelegateViews(service) {}

AppListControllerDelegateWin::~AppListControllerDelegateWin() {}

bool AppListControllerDelegateWin::ForceNativeDesktop() const {
  return true;
}

gfx::ImageSkia AppListControllerDelegateWin::GetWindowIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(GetAppListIconResourceId());
  return *resource;
}

void AppListControllerDelegateWin::FillLaunchParams(AppLaunchParams* params) {
  params->desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  extensions::AppWindow* any_existing_window =
      extensions::AppWindowRegistry::Get(params->profile)
          ->GetCurrentAppWindowForApp(params->extension_id);
  if (any_existing_window &&
      chrome::GetHostDesktopTypeForNativeWindow(
          any_existing_window->GetNativeWindow())
      != chrome::HOST_DESKTOP_TYPE_NATIVE) {
    params->desktop_type = chrome::HOST_DESKTOP_TYPE_ASH;
    chrome::ActivateMetroChrome();
  }
}
