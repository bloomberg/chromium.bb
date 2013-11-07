// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_controller_delegate_linux.h"

#include "chrome/browser/ui/views/app_list/linux/app_list_service_linux.h"

AppListControllerDelegateLinux::AppListControllerDelegateLinux(
    AppListServiceLinux* service)
    : AppListControllerDelegateImpl(service),
      service_(service) {
}

AppListControllerDelegateLinux::~AppListControllerDelegateLinux() {}

void AppListControllerDelegateLinux::ViewClosing() {
  service_->OnAppListClosing();
}

void AppListControllerDelegateLinux::OnShowExtensionPrompt() {
  service_->set_can_close(false);
}

void AppListControllerDelegateLinux::OnCloseExtensionPrompt() {
  service_->set_can_close(true);
}

bool AppListControllerDelegateLinux::CanDoCreateShortcutsFlow() {
  return true;
}
