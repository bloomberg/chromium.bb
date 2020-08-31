// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_launcher.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace apps {

BrowserAppLauncher::BrowserAppLauncher(Profile* profile) : profile_(profile) {
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions) ||
      base::FeatureList::IsEnabled(features::kDesktopPWAsUnifiedLaunch)) {
    web_app_launch_manager_ =
        std::make_unique<web_app::WebAppLaunchManager>(profile);
  }
}

BrowserAppLauncher::~BrowserAppLauncher() = default;

content::WebContents* BrowserAppLauncher::LaunchAppWithParams(
    const AppLaunchParams& params) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          params.app_id);
  if ((!extension || extension->from_bookmark()) && web_app_launch_manager_) {
    return web_app_launch_manager_->OpenApplication(params);
  }

  if (params.container ==
          apps::mojom::LaunchContainer::kLaunchContainerWindow &&
      extension && extension->from_bookmark()) {
    web_app::RecordAppWindowLaunch(profile_, params.app_id);
  }
  return ::OpenApplication(profile_, params);
}

void BrowserAppLauncher::LaunchAppWithCallback(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  // old-style app shortcuts
  if (app_id.empty()) {
    ::LaunchAppWithCallback(profile_, app_id, command_line, current_directory,
                            std::move(callback));
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);
  if ((!extension || extension->from_bookmark()) && web_app_launch_manager_) {
    web_app_launch_manager_->LaunchApplication(
        app_id, command_line, current_directory, std::move(callback));
    return;
  }

  ::LaunchAppWithCallback(profile_, app_id, command_line, current_directory,
                          std::move(callback));
}

}  // namespace apps
