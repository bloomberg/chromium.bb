// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

class ServiceWorkerTest : public ExtensionApiTest {
 public:
  // Set the channel to "trunk" since service workers are restricted to trunk.
  ServiceWorkerTest()
      : current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

  ~ServiceWorkerTest() override {}

 private:
  ScopedCurrentChannel current_channel_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, RegisterServiceWorkersOnTrunk) {
  ExtensionTestMessageListener listener(false);
  // This should fail because there are changes to be made to
  // successfully register a service worker.
  ASSERT_FALSE(RunExtensionTest("service_worker/register")) << message_;
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ(
      "SecurityError: Failed to register a ServiceWorker: No URL is "
      "associated with the caller's document.",
      listener.message());
}

// This feature is restricted to trunk, so on dev it should have existing
// behavior - which is for it to fail.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, CannotRegisterServiceWorkersOnDev) {
  ScopedCurrentChannel current_channel_override(
      chrome::VersionInfo::CHANNEL_DEV);
  ExtensionTestMessageListener listener(false);
  ASSERT_FALSE(RunExtensionTest("service_worker/register")) << message_;
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ(
      "SecurityError: Failed to register a ServiceWorker: The URL "
      "protocol of the current origin ('chrome-extension://" +
          GetSingleLoadedExtension()->id() + "') is not supported.",
      listener.message());
}

}  // namespace extensions
