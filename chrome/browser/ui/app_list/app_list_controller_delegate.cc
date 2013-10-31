// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/ui/app_list/extension_uninstaller.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "net/base/url_util.h"

namespace {

const extensions::Extension* GetExtension(Profile* profile,
                              const std::string& extension_id) {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* extension =
      service->GetInstalledExtension(extension_id);
  return extension;
}

}  // namespace

AppListControllerDelegate::~AppListControllerDelegate() {}

void AppListControllerDelegate::ViewClosing() {}

void AppListControllerDelegate::OnShowExtensionPrompt() {}
void AppListControllerDelegate::OnCloseExtensionPrompt() {}

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

bool AppListControllerDelegate::UserMayModifySettings(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile)->management_policy();
  return extension &&
         policy->UserMayModifySettings(extension, NULL);
}

void AppListControllerDelegate::UninstallApp(Profile* profile,
                                             const std::string& app_id) {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller =
      new ExtensionUninstaller(profile, app_id, this);
  uninstaller->Run();
}

bool AppListControllerDelegate::IsAppFromWebStore(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  return extension && extension->from_webstore();
}

void AppListControllerDelegate::ShowAppInWebStore(
    Profile* profile,
    const std::string& app_id,
    bool is_search_result) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  if (!extension)
    return;

  const GURL url = extensions::ManifestURL::GetDetailsURL(extension);
  DCHECK_NE(url, GURL::EmptyGURL());

  const std::string source = AppListSourceToString(
      is_search_result ?
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH :
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST);
  chrome::NavigateParams params(
      profile,
      net::AppendQueryParameter(url,
                                extension_urls::kWebstoreSourceField,
                                source),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

bool AppListControllerDelegate::HasOptionsPage(
    Profile* profile,
    const std::string& app_id) {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* extension = GetExtension(profile, app_id);
  return service->IsExtensionEnabledForLauncher(app_id) &&
         extension &&
         !extensions::ManifestURL::GetOptionsPage(extension).is_empty();
}

void AppListControllerDelegate::ShowOptionsPage(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  if (!extension)
    return;

  chrome::NavigateParams params(
      profile,
      extensions::ManifestURL::GetOptionsPage(extension),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

extensions::ExtensionPrefs::LaunchType
AppListControllerDelegate::GetExtensionLaunchType(
    Profile* profile,
    const std::string& app_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return service->extension_prefs()->
      GetLaunchType(GetExtension(profile, app_id),
                    extensions::ExtensionPrefs::LAUNCH_DEFAULT);
}

void AppListControllerDelegate::SetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id,
    extensions::ExtensionPrefs::LaunchType launch_type) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->extension_prefs()->SetLaunchType(extension_id, launch_type);
}

bool AppListControllerDelegate::IsExtensionInstalled(
    Profile* profile, const std::string& app_id) {
  return !!GetExtension(profile, app_id);
}
