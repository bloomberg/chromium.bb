// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_TEST_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_TEST_WEB_APP_PROVIDER_H_

#include <memory>

#include "chrome/browser/web_applications/web_app_provider.h"

class Profile;

namespace web_app {

class SystemWebAppManager;

class TestWebAppProvider : public WebAppProvider {
 public:
  explicit TestWebAppProvider(Profile* profile);
  ~TestWebAppProvider() override;

  void SetSystemWebAppManager(
      std::unique_ptr<SystemWebAppManager> system_web_app_manager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_TEST_WEB_APP_PROVIDER_H_
