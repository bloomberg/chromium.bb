// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionsAPIClient;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

class ExtensionViewTest : public extensions::PlatformAppBrowserTest,
                          public testing::WithParamInterface<bool> {
 public:
  ExtensionViewTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
    // before a guest is created.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  const std::string& app_to_embed,
                  const std::string& second_app_to_embed) {
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
            base::StringPrintf("runTest('%s', '%s', '%s')", test_name.c_str(),
                               app_to_embed.c_str(),
                               second_app_to_embed.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);

    bool use_cross_process_frames_for_guests = GetParam();
    if (use_cross_process_frames_for_guests) {
      command_line->AppendSwitchASCII(
          switches::kEnableFeatures,
          ::features::kGuestViewCrossProcessFrames.name);
    } else {
      command_line->AppendSwitchASCII(
          switches::kDisableFeatures,
          ::features::kGuestViewCrossProcessFrames.name);
    }
  }

  TestGuestViewManagerFactory factory_;
};

INSTANTIATE_TEST_CASE_P(ExtensionViewTests, ExtensionViewTest, testing::Bool());

// Tests that <extensionview> can be created and added to the DOM.
IN_PROC_BROWSER_TEST_P(ExtensionViewTest,
                       TestExtensionViewCreationShouldSucceed) {
  TestHelper("testExtensionViewCreationShouldSucceed",
             "extension_view/creation", "", "");
}

// Tests that verify that <extensionview> does not change extension ID if
// someone tries to change it in JavaScript.
IN_PROC_BROWSER_TEST_P(ExtensionViewTest, ShimExtensionAttribute) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("extension_view/skeleton");
  TestHelper("testExtensionAttribute", "extension_view/extension_attribute",
             skeleton_app->id(), "");
}

// Tests that verify that <extensionview> does not change src if
// someone tries to change it in JavaScript.
IN_PROC_BROWSER_TEST_P(ExtensionViewTest, ShimSrcAttribute) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("extension_view/skeleton");
  TestHelper("testSrcAttribute", "extension_view/src_attribute",
             skeleton_app->id(), "");
}

// Tests that verify that <extensionview> can call the load function.
// Flaky under MemorySanitizer: https://crbug.com/545656
IN_PROC_BROWSER_TEST_P(ExtensionViewTest, DISABLED_LoadAPICall) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("extension_view/skeleton");
  const extensions::Extension* skeleton_app_two =
      InstallPlatformApp("extension_view/skeleton_two");
  TestHelper("testLoadAPIFunction", "extension_view/load_api",
             skeleton_app->id(),
             skeleton_app_two->id());
}

// Flaky under MemorySanitizer: https://crbug.com/512092
// Flaky elsewhere: https://crbug.com/538114
// Tests that verify that <extensionview> can queue up multiple calls to the
// load function.
IN_PROC_BROWSER_TEST_P(ExtensionViewTest, DISABLED_QueuedLoadAPICall) {
  const extensions::Extension* skeleton_app =
      InstallPlatformApp("extension_view/skeleton");
  const extensions::Extension* skeleton_app_two =
      InstallPlatformApp("extension_view/skeleton_two");
  TestHelper("testQueuedLoadAPIFunction", "extension_view/load_api",
             skeleton_app->id(),
             skeleton_app_two->id());
}
