// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class VariationsServiceDevicePolicyTest : public DevicePolicyCrosBrowserTest {
 protected:
  VariationsServiceDevicePolicyTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
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
  const GURL url =
      chrome_variations::VariationsService::GetVariationsServerURL(
          g_browser_process->local_state());
  EXPECT_TRUE(StartsWithASCII(url.spec(), default_variations_url, true));
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}

} // namespace policy
