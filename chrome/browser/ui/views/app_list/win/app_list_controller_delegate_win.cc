// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
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
