// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using guest_view::GuestViewManager;
using guest_view::TestGuestViewManagerFactory;

class AppViewTest : public extensions::PlatformAppBrowserTest {
 public:
  AppViewTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  enum TestServer {
    NEEDS_TEST_SERVER,
    NO_TEST_SERVER
  };

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  const std::string& app_to_embed,
                  TestServer test_server) {
    // For serving guest pages.
    if (test_server == NEEDS_TEST_SERVER) {
      if (!StartEmbeddedTestServer()) {
        LOG(ERROR) << "FAILED TO START TEST SERVER.";
        return;
      }
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    if (!embedder_web_contents) {
      LOG(ERROR) << "UNABLE TO FIND EMBEDDER WEB CONTENTS.";
      return;
    }

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            embedder_web_contents,
            base::StringPrintf("runTest('%s', '%s')",
                               test_name.c_str(),
                               app_to_embed.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  TestGuestViewManagerFactory factory_;
};

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewWithUndefinedDataShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewWithUndefinedDataShouldSucceed",
             "app_view/shim",
             skeleton_app->id(),
             NO_TEST_SERVER);
}

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewRefusedDataShouldFail) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewRefusedDataShouldFail",
             "app_view/shim",
             skeleton_app->id(),
             NO_TEST_SERVER);
}

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewGoodDataShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewGoodDataShouldSucceed",
             "app_view/shim",
             skeleton_app->id(),
             NO_TEST_SERVER);
}

// Tests that <appview> correctly handles multiple successive connects.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewMultipleConnects) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewMultipleConnects",
             "app_view/shim",
             skeleton_app->id(),
             NO_TEST_SERVER);
}

// Tests that <appview> does not embed self (the app which owns appview).
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewEmbedSelfShouldFail) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewEmbedSelfShouldFail",
             "app_view/shim",
             skeleton_app->id(),
             NO_TEST_SERVER);
}
