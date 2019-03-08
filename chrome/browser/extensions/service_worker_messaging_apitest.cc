// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

enum BindingsType { NATIVE_BINDINGS, JAVASCRIPT_BINDINGS };

// Returns the newly added WebContents.
content::WebContents* AddTab(Browser* browser, const GURL& url) {
  int starting_tab_count = browser->tab_strip_model()->count();
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(starting_tab_count + 1, browser->tab_strip_model()->count());
  return browser->tab_strip_model()->GetActiveWebContents();
}

}  // namespace

class ServiceWorkerMessagingTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<BindingsType> {
 public:
  ServiceWorkerMessagingTest()
      : current_channel_(
            // Extensions APIs from SW are only enabled on trunk.
            // It is important to set the channel early so that this change is
            // visible in renderers running with service workers (and no
            // extension).
            version_info::Channel::UNKNOWN) {}
  ~ServiceWorkerMessagingTest() override = default;

  void SetUp() override {
    if (GetParam() == NATIVE_BINDINGS) {
      scoped_feature_list_.InitAndEnableFeature(
          extensions_features::kNativeCrxBindings);
    } else {
      DCHECK_EQ(JAVASCRIPT_BINDINGS, GetParam());
      scoped_feature_list_.InitAndDisableFeature(
          extensions_features::kNativeCrxBindings);
    }
    ExtensionApiTest::SetUp();
  }

 private:
  ScopedCurrentChannel current_channel_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessagingTest);
};

// Tests one-way message from content script to SW extension using
// chrome.runtime.sendMessage.
IN_PROC_BROWSER_TEST_P(ServiceWorkerMessagingTest, TabToWorkerOneWay) {
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING", false);
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker_one_way"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener test_listener("WORKER_RECEIVED_MESSAGE", false);
  test_listener.set_failure_message("FAILURE");

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents = AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
}

// Tests chrome.runtime.sendMessage from content script to SW extension.
IN_PROC_BROWSER_TEST_P(ServiceWorkerMessagingTest, TabToWorker) {
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING", false);
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener reply_listener("CONTENT_SCRIPT_RECEIVED_REPLY",
                                              false);
  reply_listener.set_failure_message("FAILURE");

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents = AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(reply_listener.WaitUntilSatisfied());
}

// Tests chrome.tabs.sendMessage from SW extension to content script.
IN_PROC_BROWSER_TEST_P(ServiceWorkerMessagingTest, WorkerToTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("service_worker/messaging/send_message_worker_to_tab"))
      << message_;
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension SW and disconnecting the port.
IN_PROC_BROWSER_TEST_P(ServiceWorkerMessagingTest,
                       TabToWorker_ConnectAndDisconnect) {
  // Load an extension that will inject content script to |new_web_contents|.
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/connect_and_disconnect"));
  ASSERT_TRUE(extension);

  // Load the tab with content script to open a Port to |extension|.
  // Test concludes when extension gets notified about port being disconnected.
  ResultCatcher catcher;
  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    content::WebContents* new_web_contents =
        AddTab(browser(),
               embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension and sending message through the port.
// TODO(lazyboy): Refactor common parts with TabToWorker_ConnectAndDisconnect.
IN_PROC_BROWSER_TEST_P(ServiceWorkerMessagingTest,
                       TabToWorker_ConnectAndPostMessage) {
  // Load an extension that will inject content script to |new_web_contents|.
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/post_message"));
  ASSERT_TRUE(extension);

  // Load the tab with content script to send message to |extension| via port.
  // Test concludes when the content script receives a reply.
  ResultCatcher catcher;
  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    content::WebContents* new_web_contents =
        AddTab(browser(),
               embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

INSTANTIATE_TEST_SUITE_P(ServiceWorkerMessagingTestWithNativeBindings,
                         ServiceWorkerMessagingTest,
                         ::testing::Values(NATIVE_BINDINGS));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerMessagingTestWithJSBindings,
                         ServiceWorkerMessagingTest,
                         ::testing::Values(JAVASCRIPT_BINDINGS));
}  // namespace extensions
