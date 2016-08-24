// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/variations/service/variations_service.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class VariationsServiceDevicePolicyTest : public DevicePolicyCrosBrowserTest {
 protected:
  VariationsServiceDevicePolicyTest() {}

  void SetUpInProcessBrowserTestFixture() override {
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
      variations::VariationsService::GetDefaultVariationsServerURLForTesting();

  // g_browser_process->variations_service() is null by default in Chromium
  // builds, so construct a VariationsService locally instead.
  std::unique_ptr<variations::VariationsService> service =
      variations::VariationsService::CreateForTesting(
          base::MakeUnique<ChromeVariationsServiceClient>(),
          g_browser_process->local_state());

  // Device policy has updated the cros settings.
  const GURL url = service->GetVariationsServerURL(
      g_browser_process->local_state(), std::string());
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}

}  // namespace policy
