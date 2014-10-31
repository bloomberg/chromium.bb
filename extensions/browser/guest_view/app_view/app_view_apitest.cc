// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/browser/guest_view/test_guest_view_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/filename_util.h"

namespace extensions {

class AppViewTest : public AppShellTest {
 protected:
  AppViewTest() { GuestViewManager::set_factory_for_testing(&factory_); }

  TestGuestViewManager* GetGuestViewManager() {
    return static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(
            ShellContentBrowserClient::Get()->GetBrowserContext()));
  }

  content::WebContents* GetFirstAppWindowWebContents() {
    const AppWindowRegistry::AppWindowList& app_window_list =
        AppWindowRegistry::Get(browser_context_)->app_windows();
    DCHECK(app_window_list.size() == 1);
    return (*app_window_list.begin())->web_contents();
  }

  const Extension* LoadApp(const std::string& app_location) {
    base::FilePath test_data_dir;
    PathService::Get(DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(app_location.c_str());
    return extension_system_->LoadApp(test_data_dir);
  }

  void RunTest(const std::string& test_name,
               const std::string& app_location,
               const std::string& app_to_embed) {
    extension_system_->Init();

    const Extension* app_embedder = LoadApp(app_location);
    ASSERT_TRUE(app_embedder);
    const Extension* app_embedded = LoadApp(app_to_embed);
    ASSERT_TRUE(app_embedded);

    extension_system_->LaunchApp(app_embedder->id());

    ExtensionTestMessageListener launch_listener("LAUNCHED", false);
    ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

    embedder_web_contents_ = GetFirstAppWindowWebContents();

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    ASSERT_TRUE(
        content::ExecuteScript(embedder_web_contents_,
                               base::StringPrintf("runTest('%s', '%s')",
                                                  test_name.c_str(),
                                                  app_embedded->id().c_str())))
        << "Unable to start test.";
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

 protected:
  content::WebContents* embedder_web_contents_;
  TestGuestViewManagerFactory factory_;
};

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewGoodDataShouldSucceed) {
  RunTest("testAppViewGoodDataShouldSucceed",
          "app_view/apitest",
          "app_view/apitest/skeleton");
}

// Tests that <appview> correctly processes parameters passed on connect.
// This test should fail to connect because the embedded app (skeleton) will
// refuse the data passed by the embedder app and deny the request.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewRefusedDataShouldFail) {
  RunTest("testAppViewRefusedDataShouldFail",
          "app_view/apitest",
          "app_view/apitest/skeleton");
}

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewWithUndefinedDataShouldSucceed) {
  RunTest("testAppViewWithUndefinedDataShouldSucceed",
          "app_view/apitest",
          "app_view/apitest/skeleton");
}

}  // namespace extensions
