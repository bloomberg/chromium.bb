// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

// A test fixture simulating a browser that is already managed.
class DeviceCloudPolicyManagedBrowserTest : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceCloudPolicyManagedBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
    RefreshDevicePolicy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagedBrowserTest);
};

IN_PROC_BROWSER_TEST_F(DeviceCloudPolicyManagedBrowserTest, NoInitializer) {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  EXPECT_FALSE(connector->GetDeviceCloudPolicyInitializer());
}

// A test fixture simulating a browser that is still unmanaged.
class DeviceCloudPolicyUnmanagedBrowserTest
    : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceCloudPolicyUnmanagedBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyUnmanagedBrowserTest);
};

IN_PROC_BROWSER_TEST_F(DeviceCloudPolicyUnmanagedBrowserTest,
                       StillHasInitializer) {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());
}

}  // namespace
}  // namespace policy
