// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

using namespace policy;

class ManagedUiTest : public InProcessBrowserTest {
 public:
  ManagedUiTest() = default;
  ~ManagedUiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
  }

  MockConfigurationPolicyProvider* provider() { return &provider_; }

 private:
  MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUiTest);
};

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("", "ShowManagedUi");

  PolicyMap policy_map;
  policy_map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>("hello world"), nullptr);
  provider()->UpdateChromePolicy(policy_map);

  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiNoPolicies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("ShowManagedUi", "");

  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiOnDesktop) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("ShowManagedUi", "");

  PolicyMap policy_map;
  policy_map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>("hello world"), nullptr);
  provider()->UpdateChromePolicy(policy_map);

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#else
  EXPECT_TRUE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#endif
}
