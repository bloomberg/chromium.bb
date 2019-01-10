// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_

#include <memory>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class KeyedService;

namespace content {
class BrowserContext;
}

namespace web_app {

class TestSystemWebAppManager;
class TestWebUIControllerFactory;

class SystemWebAppManagerBrowserTest : public InProcessBrowserTest {
 public:
  SystemWebAppManagerBrowserTest();
  ~SystemWebAppManagerBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;

 protected:
  Browser* WaitForSystemAppInstallAndLaunch();

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::unique_ptr<KeyedService> CreateWebAppProvider(
      content::BrowserContext* context);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebUIControllerFactory> factory_;
  TestSystemWebAppManager* test_system_web_app_manager_ = nullptr;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerBrowserTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
