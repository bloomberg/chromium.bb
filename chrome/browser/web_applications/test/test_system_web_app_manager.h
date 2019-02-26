// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class TestSystemWebAppManager : public SystemWebAppManager {
 public:
  TestSystemWebAppManager(Profile* profile,
                          PendingAppManager* pending_app_manager);
  ~TestSystemWebAppManager() override;

  void SetSystemApps(base::flat_map<SystemAppType, GURL> system_apps);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_MANAGER_H_
