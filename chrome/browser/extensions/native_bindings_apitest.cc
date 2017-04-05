// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

// And end-to-end test for extension APIs using native bindings.
class NativeBindingsApiTest : public ExtensionApiTest {
 public:
  NativeBindingsApiTest() {}
  ~NativeBindingsApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // We whitelist the extension so that it can use the cast.streaming.* APIs,
    // which are the only APIs that are prefixed twice.
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
    // Note: We don't use a FeatureSwitch::ScopedOverride here because we need
    // the switch to be propogated to the renderer, which doesn't happen with
    // a ScopedOverride.
    command_line->AppendSwitchASCII(switches::kNativeCrxBindings, "1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeBindingsApiTest);
};

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleEndToEndTest) {
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("native_bindings/extension")) << message_;
}

// A simplistic app test for app-specific APIs.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleAppTest) {
  ASSERT_TRUE(RunPlatformAppTest("native_bindings/platform_app")) << message_;
}

// Tests the declarativeContent API and declarative events.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, DeclarativeEvents) {
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an extension and wait for it to be ready.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_bindings/declarative_content"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // The extension's page action should currently be hidden.
  ExtensionAction* page_action =
      ExtensionActionManager::Get(profile())->GetPageAction(*extension);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));

  // Navigating to example.com should show the page action.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));

  // And the extension should be notified of the click.
  ExtensionTestMessageListener clicked_listener("clicked and removed", false);
  ExtensionActionAPI::Get(profile())->DispatchExtensionActionClicked(
      *page_action, web_contents);
  ASSERT_TRUE(clicked_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, LazyListeners) {
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  LazyBackgroundObserver background_page_done;
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_bindings/lazy_listeners"));
  ASSERT_TRUE(extension);
  background_page_done.Wait();

  EventRouter* event_router = EventRouter::Get(profile());
  EXPECT_TRUE(event_router->ExtensionHasEventListener(extension->id(),
                                                      "tabs.onCreated"));
}

// End-to-end test for the fileSystem API, which includes parameters with
// instance-of requirements and a post-validation argument updater that violates
// the schema.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, FileSystemApiGetDisplayPath) {
  base::FilePath test_dir = test_data_dir_.AppendASCII("native_bindings");
  FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
      "test_root", test_dir);
  base::FilePath test_file = test_dir.AppendASCII("text.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("native_bindings/instance_of")) << message_;
}

}  // namespace extensions
