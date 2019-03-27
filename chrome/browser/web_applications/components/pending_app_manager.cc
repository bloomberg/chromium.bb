// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/stl_util.h"

namespace web_app {

PendingAppManager::PendingAppManager() = default;

PendingAppManager::~PendingAppManager() = default;

void PendingAppManager::SynchronizeInstalledApps(
    std::vector<InstallOptions> desired_apps_install_options,
    InstallSource install_source) {
  DCHECK(std::all_of(desired_apps_install_options.begin(),
                     desired_apps_install_options.end(),
                     [&install_source](const InstallOptions& install_options) {
                       return install_options.install_source == install_source;
                     }));

  std::vector<GURL> current_urls = GetInstalledAppUrls(install_source);
  std::sort(current_urls.begin(), current_urls.end());

  std::vector<GURL> desired_urls;
  for (const auto& info : desired_apps_install_options) {
    desired_urls.emplace_back(info.url);
  }
  std::sort(desired_urls.begin(), desired_urls.end());

  UninstallApps(
      base::STLSetDifference<std::vector<GURL>>(current_urls, desired_urls),
      base::DoNothing());
  InstallApps(std::move(desired_apps_install_options), base::DoNothing());
}

}  // namespace web_app
