// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class VariationsServiceDevicePolicyTest : public DevicePolicyCrosBrowserTest {
 protected:
  VariationsServiceDevicePolicyTest() {}

  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    InstallOwnerKey();
    SetSpecificDevicePolicies();
    RefreshDevicePolicy();
  }

  void SetSpecificDevicePolicies() {
    // Setup the device policy DeviceVariationsRestrictParameter.
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    proto.mutable_variations_parameter()->set_parameter("restricted");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VariationsServiceDevicePolicyTest);
};

IN_PROC_BROWSER_TEST_F(VariationsServiceDevicePolicyTest, VariationsURLValid) {
  const std::string default_variations_url =
      chrome_variations::VariationsService::
          GetDefaultVariationsServerURLForTesting();

  // Device policy has updated the cros settings.
  EXPECT_EQ(default_variations_url + "?restrict=restricted",
            chrome_variations::VariationsService::GetVariationsServerURL(
                TestingBrowserProcess::GetGlobal()->local_state()).spec());
}

} // namespace policy
