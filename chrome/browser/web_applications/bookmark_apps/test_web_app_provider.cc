// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/test_web_app_provider.h"

#include "chrome/browser/web_applications/system_web_app_manager.h"

namespace web_app {

TestWebAppProvider::TestWebAppProvider(Profile* profile)
    : WebAppProvider(profile) {}

TestWebAppProvider::~TestWebAppProvider() = default;

void TestWebAppProvider::SetSystemWebAppManager(
    std::unique_ptr<SystemWebAppManager> system_web_app_manager) {
  system_web_app_manager_ = std::move(system_web_app_manager);
}

}  // namespace web_app
