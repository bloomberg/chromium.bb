// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"

namespace web_app {

TestSystemWebAppManager::TestSystemWebAppManager(
    Profile* profile,
    PendingAppManager* pending_app_manager)
    : SystemWebAppManager(profile, pending_app_manager) {}

TestSystemWebAppManager::~TestSystemWebAppManager() = default;

void TestSystemWebAppManager::SetSystemApps(std::vector<GURL> system_apps) {
  system_apps_ = std::move(system_apps);
}

std::vector<GURL> TestSystemWebAppManager::CreateSystemWebApps() {
  return std::move(system_apps_);
}

}  // namespace web_app
