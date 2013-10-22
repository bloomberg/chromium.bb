// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/gfx/image/image_skia.h"

AppListControllerDelegate::~AppListControllerDelegate() {}

void AppListControllerDelegate::ViewClosing() {}

gfx::ImageSkia AppListControllerDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

bool AppListControllerDelegate::IsAppPinned(const std::string& extension_id) {
  return false;
}

void AppListControllerDelegate::PinApp(const std::string& extension_id) {}

void AppListControllerDelegate::UnpinApp(const std::string& extension_id) {}

bool AppListControllerDelegate::CanDoCreateShortcutsFlow() {
  return false;
}

void AppListControllerDelegate::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  DCHECK(CanDoCreateShortcutsFlow());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  gfx::NativeWindow parent_window = GetAppListWindow();
  if (!parent_window)
    return;
  OnShowExtensionPrompt();
  chrome::ShowCreateChromeAppShortcutsDialog(
      parent_window, profile, extension,
      base::Bind(&AppListControllerDelegate::OnCloseExtensionPrompt,
                 base::Unretained(this)));
}

void AppListControllerDelegate::CreateNewWindow(Profile* profile,
                                                bool incognito) {
  NOTREACHED();
}

bool AppListControllerDelegate::ShouldShowUserIcon() {
  return g_browser_process->profile_manager()->GetNumberOfProfiles() > 1;
}

std::string AppListControllerDelegate::AppListSourceToString(
    AppListSource source) {
  switch (source) {
    case LAUNCH_FROM_APP_LIST:
      return extension_urls::kLaunchSourceAppList;
    case LAUNCH_FROM_APP_LIST_SEARCH:
      return extension_urls::kLaunchSourceAppListSearch;
    default: return std::string();
  }
}
