// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

class TestGuestViewManager : public extensions::GuestViewManager {
 public:
  explicit TestGuestViewManager(content::BrowserContext* context) :
      extensions::GuestViewManager(context),
      web_contents_(NULL) {}

  content::WebContents* WaitForGuestCreated() {
    if (web_contents_)
      return web_contents_;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return web_contents_;
  }

 private:
  // GuestViewManager override:
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents) OVERRIDE{
    extensions::GuestViewManager::AddGuest(
        guest_instance_id, guest_web_contents);
    web_contents_ = guest_web_contents;

    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public extensions::GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory() :
      test_guest_view_manager_(NULL) {}

  virtual ~TestGuestViewManagerFactory() {}

  virtual extensions::GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) OVERRIDE {
    return GetManager(context);
  }

  TestGuestViewManager* GetManager(content::BrowserContext* context) {
    if (!test_guest_view_manager_) {
      test_guest_view_manager_ = new TestGuestViewManager(context);
    }
    return test_guest_view_manager_;
  }

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

}  // namespace

class AppViewTest : public extensions::PlatformAppBrowserTest {
 public:
  AppViewTest() {
    extensions::GuestViewManager::set_factory_for_testing(&factory_);
  }

  TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetManager(browser()->profile());
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
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableAppView);
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  TestGuestViewManagerFactory factory_;
};

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewBasic) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("app_view/shim/skeleton");
  TestHelper("testAppViewBasic",
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
