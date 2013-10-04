// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_controller_delegate_win.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/app_list/app_list_icon_win.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/views/app_list/win/app_list_service_win.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"

AppListControllerDelegateWin::AppListControllerDelegateWin(
    AppListServiceWin* service)
    : service_(service) {
}

AppListControllerDelegateWin::~AppListControllerDelegateWin() {}

void AppListControllerDelegateWin::DismissView() {
  service_->DismissAppList();
}

void AppListControllerDelegateWin::ViewClosing() {
  service_->OnAppListClosing();
}

gfx::NativeWindow AppListControllerDelegateWin::GetAppListWindow() {
  return service_->GetAppListWindow();
}

gfx::ImageSkia AppListControllerDelegateWin::GetWindowIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(GetAppListIconResourceId());
  return *resource;
}

AppListControllerDelegate::Pinnable
    AppListControllerDelegateWin::GetPinnable() {
  return NO_PIN;
}

void AppListControllerDelegateWin::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  service_->SetProfilePath(profile_path);
  service_->Show();
}

void AppListControllerDelegateWin::OnShowExtensionPrompt() {
  service_->set_can_close(false);
}

void AppListControllerDelegateWin::OnCloseExtensionPrompt() {
  service_->set_can_close(true);
}

bool AppListControllerDelegateWin::CanDoCreateShortcutsFlow(
    bool is_platform_app) {
  return true;
}

void AppListControllerDelegateWin::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  gfx::NativeWindow parent_hwnd = GetAppListWindow();
  if (!parent_hwnd)
    return;
  OnShowExtensionPrompt();
  chrome::ShowCreateChromeAppShortcutsDialog(
      parent_hwnd, profile, extension,
      base::Bind(&AppListControllerDelegateWin::OnCloseExtensionPrompt,
                 base::Unretained(this)));
}

void AppListControllerDelegateWin::CreateNewWindow(Profile* profile,
                                                   bool incognito) {
  Profile* window_profile = incognito ?
      profile->GetOffTheRecordProfile() : profile;
  chrome::NewEmptyWindow(window_profile, chrome::GetActiveDesktop());
}

void AppListControllerDelegateWin::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  LaunchApp(profile, extension, source, event_flags);
}

void AppListControllerDelegateWin::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  AppListServiceImpl::RecordAppListAppLaunch();

  AppLaunchParams params(profile, extension, NEW_FOREGROUND_TAB);

  if (source != LAUNCH_FROM_UNKNOWN &&
      extension->id() == extension_misc::kWebStoreAppId) {
    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url,
        extension_urls::kWebstoreSourceField,
        AppListSourceToString(source));
  }

  OpenApplication(params);
}

