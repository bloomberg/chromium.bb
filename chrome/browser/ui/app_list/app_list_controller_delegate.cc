// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include "base/logging.h"

AppListControllerDelegate::~AppListControllerDelegate() {}

void AppListControllerDelegate::ViewClosing() {}

void AppListControllerDelegate::ViewActivationChanged(bool active) {}

bool AppListControllerDelegate::IsAppPinned(const std::string& extension_id) {
  return false;
}

void AppListControllerDelegate::PinApp(const std::string& extension_id) {}

void AppListControllerDelegate::UnpinApp(const std::string& extension_id) {}

void AppListControllerDelegate::ShowCreateShortcutsDialog(
    Profile* profile,
    const std::string& extension_id) {}

void AppListControllerDelegate::CreateNewWindow(bool incognito) {
  NOTREACHED();
}
