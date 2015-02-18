// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

class TestGuestViewManager : public extensions::GuestViewManager {
 public:
  explicit TestGuestViewManager(content::BrowserContext* context)
      : extensions::GuestViewManager(context), web_contents_(NULL) {}

  content::WebContents* WaitForGuestCreated() {
    if (web_contents_)
      return web_contents_;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return web_contents_;
  }

 private:
  // GuestViewManager override.
  void AddGuest(int guest_instance_id,
                content::WebContents* guest_web_contents) override {
    extensions::GuestViewManager::AddGuest(guest_instance_id,
                                           guest_web_contents);
    web_contents_ = guest_web_contents;

    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public extensions::GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory() : test_guest_view_manager_(NULL) {}

  ~TestGuestViewManagerFactory() override {}

  extensions::GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) override {
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

class ExtensionViewTest : public extensions::PlatformAppBrowserTest {
 public:
  ExtensionViewTest() {
    extensions::GuestViewManager::set_factory_for_testing(&factory_);
  }

  TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetManager(browser()->profile());
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

  TestGuestViewManagerFactory factory_;
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
