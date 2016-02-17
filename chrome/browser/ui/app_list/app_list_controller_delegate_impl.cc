// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"
#include "ui/gfx/image/image_skia.h"

AppListControllerDelegateImpl::AppListControllerDelegateImpl(
    AppListService* service)
    : service_(service) {}

AppListControllerDelegateImpl::~AppListControllerDelegateImpl() {}

void AppListControllerDelegateImpl::DismissView() {
  service_->DismissAppList();
}

gfx::NativeWindow AppListControllerDelegateImpl::GetAppListWindow() {
  return service_->GetAppListWindow();
}

gfx::ImageSkia AppListControllerDelegateImpl::GetWindowIcon() {
  return gfx::ImageSkia();
}

bool AppListControllerDelegateImpl::IsAppPinned(
    const std::string& extension_id) {
  return false;
}

void AppListControllerDelegateImpl::PinApp(const std::string& extension_id) {
  NOTREACHED();
}

void AppListControllerDelegateImpl::UnpinApp(const std::string& extension_id) {
  NOTREACHED();
}

AppListControllerDelegateImpl::Pinnable
AppListControllerDelegateImpl::GetPinnable(const std::string& extension_id) {
  return NO_PIN;
}

bool AppListControllerDelegateImpl::CanDoCreateShortcutsFlow() {
  return false;
}

void AppListControllerDelegateImpl::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  DCHECK(CanDoCreateShortcutsFlow());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension = registry->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  gfx::NativeWindow parent_window = GetAppListWindow();
  if (!parent_window)
    return;
  OnShowChildDialog();
  chrome::ShowCreateChromeAppShortcutsDialog(
      parent_window, profile, extension,
      base::Bind(&AppListControllerDelegateImpl::OnCloseCreateShortcutsPrompt,
                 base::Unretained(this)));
}

void AppListControllerDelegateImpl::CreateNewWindow(Profile* profile,
                                                   bool incognito) {
  Profile* window_profile = incognito ?
      profile->GetOffTheRecordProfile() : profile;
  chrome::NewEmptyWindow(window_profile);
}

void AppListControllerDelegateImpl::OpenURL(Profile* profile,
                                            const GURL& url,
                                            ui::PageTransition transition,
                                            WindowOpenDisposition disposition) {
  chrome::NavigateParams params(profile, url, transition);
  params.disposition = disposition;
  chrome::Navigate(&params);
}

void AppListControllerDelegateImpl::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  LaunchApp(profile, extension, source, event_flags);
}

void AppListControllerDelegateImpl::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  AppListServiceImpl::RecordAppListAppLaunch();

  AppLaunchParams params(profile, extension, NEW_FOREGROUND_TAB,
                         extensions::SOURCE_APP_LAUNCHER);

  if (source != LAUNCH_FROM_UNKNOWN &&
      extension->id() == extensions::kWebStoreAppId) {
    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url,
        extension_urls::kWebstoreSourceField,
        AppListSourceToString(source));
  }

  FillLaunchParams(&params);
  OpenApplication(params);
}

void AppListControllerDelegateImpl::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  service_->SetProfilePath(profile_path);
  service_->Show();
}

bool AppListControllerDelegateImpl::ShouldShowUserIcon() {
  return g_browser_process->profile_manager()->GetNumberOfProfiles() > 1;
}

void AppListControllerDelegateImpl::FillLaunchParams(AppLaunchParams* params) {}

void AppListControllerDelegateImpl::OnCloseCreateShortcutsPrompt(
    bool created) {
  OnCloseChildDialog();
}
