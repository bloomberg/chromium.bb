// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_TEST_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_TEST_PENDING_APP_MANAGER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"

namespace web_app {

class TestPendingAppManager : public PendingAppManager {
 public:
  TestPendingAppManager();
  ~TestPendingAppManager() override;
  const std::vector<AppInfo>& installed_apps() const { return installed_apps_; }

  // PendingAppManager:
  void Install(AppInfo app_to_install, OnceInstallCallback callback) override;
  void InstallApps(std::vector<AppInfo> apps_to_install,
                   const RepeatingInstallCallback& callback) override;

 private:
  std::vector<AppInfo> installed_apps_;

  DISALLOW_COPY_AND_ASSIGN(TestPendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_TEST_PENDING_APP_MANAGER_H_
