// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/browser/guest_view/test_guest_view_manager.h"
#include "extensions/test/extension_test_message_listener.h"

using extensions::ExtensionsGuestViewManagerDelegate;
using extensions::GuestViewManager;
using extensions::TestGuestViewManager;

class ExtensionViewTest : public extensions::PlatformAppBrowserTest {
 public:
  ExtensionViewTest() {
    extensions::GuestViewManager::set_factory_for_testing(&factory_);
  }

  extensions::TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
    // before a guest is created.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              scoped_ptr<guestview::GuestViewManagerDelegate>(
                  new ExtensionsGuestViewManagerDelegate(
                      browser()->profile()))));
    }
    return manager;
  }

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  const std::string& app_to_embed) {
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
            base::StringPrintf("runTest('%s', '%s')", test_name.c_str(),
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

  extensions::TestGuestViewManagerFactory factory_;
};

// Tests that <extensionview> can be created and added to the DOM.
IN_PROC_BROWSER_TEST_F(ExtensionViewTest,
                       TestExtensionViewCreationShouldSucceed) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("extension_view/skeleton");
  TestHelper("testExtensionViewCreationShouldSucceed", "extension_view",
             skeleton_app->id());
}

// Tests that verify that <extensionview> can navigate to different sources.
IN_PROC_BROWSER_TEST_F(ExtensionViewTest, ShimSrcAttribute) {
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/extension_view/src_attribute"));
}

// Tests that verify that <extensionview> can call the connect function.
IN_PROC_BROWSER_TEST_F(ExtensionViewTest, ConnectAPICall) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/extension_view/connect_api"));
}

// Tests that verify that <extensionview> does not change extension ID if
// someone tries to change it in JavaScript.
IN_PROC_BROWSER_TEST_F(ExtensionViewTest, ShimExtensionAttribute) {
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/extension_view/extension_attribute"));
}
