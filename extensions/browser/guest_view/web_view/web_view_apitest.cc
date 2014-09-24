// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/browser/guest_view/web_view/test_guest_view_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

// This class intercepts download request from the guest.
class WebViewAPITest : public AppShellTest {
 protected:
  void RunTest(const std::string& test_name, const std::string& app_location) {
    base::FilePath test_data_dir;
    PathService::Get(DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(app_location.c_str());

    ASSERT_TRUE(extension_system_->LoadApp(test_data_dir));
    extension_system_->LaunchApp();

    ExtensionTestMessageListener launch_listener("LAUNCHED", false);
    ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

    const AppWindowRegistry::AppWindowList& app_window_list =
        AppWindowRegistry::Get(browser_context_)->app_windows();
    DCHECK(app_window_list.size() == 1);
    content::WebContents* embedder_web_contents =
        (*app_window_list.begin())->web_contents();

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            embedder_web_contents,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "Unable to start test.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  WebViewAPITest() { GuestViewManager::set_factory_for_testing(&factory_); }

 private:
  TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAllowTransparencyAttribute) {
  RunTest("testAllowTransparencyAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAPIMethodExistence) {
  RunTest("testAPIMethodExistence", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeAfterNavigation) {
  RunTest("testAutosizeAfterNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeBeforeNavigation) {
  RunTest("testAutosizeBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeHeight) {
  RunTest("testAutosizeHeight", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeRemoveAttributes) {
  RunTest("testAutosizeRemoveAttributes", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeWithPartialAttributes) {
  RunTest("testAutosizeWithPartialAttributes", "web_view/apitest");
}

}  // namespace extensions
