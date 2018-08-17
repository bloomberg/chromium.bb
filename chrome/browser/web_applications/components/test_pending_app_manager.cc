// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/test_pending_app_manager.h"

#include "base/callback.h"
#include "url/gurl.h"

namespace web_app {

namespace {
const char kFakeAppId[] = "lolcjfkgbenioifhfnehnlkogckoebae";
}  // namespace

TestPendingAppManager::TestPendingAppManager() = default;

TestPendingAppManager::~TestPendingAppManager() = default;

void TestPendingAppManager::Install(AppInfo app_to_install,
                                    OnceInstallCallback callback) {
  // TODO(jlklein): Add error simulation when error codes are added to the API.
  // Copy the URL before moving it so it can be passed in the callback.
  const GURL url(app_to_install.url);
  installed_apps_.push_back(std::move(app_to_install));
  std::move(callback).Run(url, std::string(kFakeAppId));
}

void TestPendingAppManager::InstallApps(
    std::vector<AppInfo> apps_to_install,
    const RepeatingInstallCallback& callback) {
  for (auto& app : apps_to_install)
    Install(std::move(app), callback);
}

}  // namespace web_app
