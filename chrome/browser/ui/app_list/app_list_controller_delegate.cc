// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/gfx/image/image_skia.h"

AppListControllerDelegate::~AppListControllerDelegate() {}

void AppListControllerDelegate::ViewClosing() {}

void AppListControllerDelegate::ViewActivationChanged(bool active) {}

gfx::ImageSkia AppListControllerDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

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

bool AppListControllerDelegate::ShouldShowUserIcon() {
  return g_browser_process->profile_manager()->GetNumberOfProfiles() > 1;
}
