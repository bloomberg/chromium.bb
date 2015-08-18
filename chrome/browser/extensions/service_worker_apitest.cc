// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

class ServiceWorkerTest : public ExtensionApiTest {
 public:
  // Set the channel to "trunk" since service workers are restricted to trunk.
  ServiceWorkerTest()
      : current_channel_(version_info::Channel::UNKNOWN) {}

  ~ServiceWorkerTest() override {}

 private:
  ScopedCurrentChannel current_channel_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, RegisterServiceWorkersOnTrunk) {
  ExtensionTestMessageListener listener(false);
  ASSERT_TRUE(RunExtensionTest("service_worker/register")) << message_;
}

// This feature is restricted to trunk, so on dev it should have existing
// behavior - which is for it to fail.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, CannotRegisterServiceWorkersOnDev) {
  ScopedCurrentChannel current_channel_override(
      version_info::Channel::DEV);
  ExtensionTestMessageListener listener(false);
  ASSERT_FALSE(RunExtensionTest("service_worker/register")) << message_;
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ(
      "SecurityError: Failed to register a ServiceWorker: The URL "
      "protocol of the current origin ('chrome-extension://" +
          GetSingleLoadedExtension()->id() + "') is not supported.",
      listener.message());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, ServiceWorkerFetchEvent) {
  RunExtensionTest("service_worker/fetch");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(contents);

  std::string output;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents, "window.domAutomationController.send(document.body.innerText);",
      &output));
  EXPECT_EQ("No Fetch Event yet.", output);

  // Page must reload in order for the service worker to take control.
  contents->GetController().Reload(true);
  content::WaitForLoadStop(contents);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents, "window.domAutomationController.send(document.body.innerText);",
      &output));
  EXPECT_EQ("Caught a fetch!", output);
}

// Binding that was created on the v8::Context of the worker for testing
// purposes should bind an empty object to chrome.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, ServiceWorkerChromeBinding) {
  ASSERT_TRUE(RunExtensionTest("service_worker/bindings")) << message_;
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(contents);

  std::string output;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents, "window.domAutomationController.send(document.body.innerText);",
      &output));
  EXPECT_EQ("No Fetch Event yet.", output);

  // Page must reload in order for the service worker to take control.
  contents->GetController().Reload(true);
  content::WaitForLoadStop(contents);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents, "window.domAutomationController.send(document.body.innerText);",
      &output));
  EXPECT_EQ("object", output);
}

}  // namespace extensions
