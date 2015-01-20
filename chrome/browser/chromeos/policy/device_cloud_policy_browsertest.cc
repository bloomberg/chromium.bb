// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceCloudPolicyBrowserTest : public InProcessBrowserTest {
 public:
  DeviceCloudPolicyBrowserTest() : mock_client_(new MockCloudPolicyClient) {
  }

  MockCloudPolicyClient* mock_client_;
};

IN_PROC_BROWSER_TEST_F(DeviceCloudPolicyBrowserTest, Initializer) {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  // Initializer exists at first.
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());

  // Initializer is deleted when the manager connects.
  connector->GetDeviceCloudPolicyManager()->StartConnection(
      make_scoped_ptr(mock_client_),
      connector->GetInstallAttributes());
  EXPECT_FALSE(connector->GetDeviceCloudPolicyInitializer());

  // Initializer is restarted when the manager disconnects.
  connector->GetDeviceCloudPolicyManager()->Disconnect();
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());
}

}  // namespace policy
