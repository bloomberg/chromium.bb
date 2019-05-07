// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/ui/webui/welcome/nux_helper.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

class StartupBrowserPolicyUnitTest : public testing::Test {
 protected:
  StartupBrowserPolicyUnitTest() = default;
  ~StartupBrowserPolicyUnitTest() override = default;

  template <typename... Args>
  std::unique_ptr<policy::PolicyMap> MakePolicy(const std::string& policy,
                                                Args... args) {
    auto policy_map = std::make_unique<policy::PolicyMap>();
    policy_map->Set(policy, policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(args...), nullptr);
    return policy_map;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserPolicyUnitTest);
};

TEST_F(StartupBrowserPolicyUnitTest, BookmarkBarEnabled) {
  EXPECT_TRUE(nux::CanShowGoogleAppModuleForTesting(policy::PolicyMap()));

  auto policy_map = MakePolicy(policy::key::kBookmarkBarEnabled, true);
  EXPECT_TRUE(nux::CanShowGoogleAppModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kBookmarkBarEnabled, false);
  EXPECT_FALSE(nux::CanShowGoogleAppModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, EditBookmarksEnabled) {
  EXPECT_TRUE(nux::CanShowGoogleAppModuleForTesting(policy::PolicyMap()));

  auto policy_map = MakePolicy(policy::key::kEditBookmarksEnabled, true);
  EXPECT_TRUE(nux::CanShowGoogleAppModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kEditBookmarksEnabled, false);
  EXPECT_FALSE(nux::CanShowGoogleAppModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, DefaultBrowserSettingEnabled) {
  EXPECT_TRUE(nux::CanShowSetDefaultModuleForTesting(policy::PolicyMap()));

  auto policy_map =
      MakePolicy(policy::key::kDefaultBrowserSettingEnabled, true);
  EXPECT_TRUE(nux::CanShowSetDefaultModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kDefaultBrowserSettingEnabled, false);
  EXPECT_FALSE(nux::CanShowSetDefaultModuleForTesting(*policy_map));
}

TEST_F(StartupBrowserPolicyUnitTest, BrowserSignin) {
  EXPECT_TRUE(nux::CanShowSigninModuleForTesting(policy::PolicyMap()));

  auto policy_map =
      MakePolicy(policy::key::kBrowserSignin,
                 static_cast<int>(policy::BrowserSigninMode::kEnabled));
  EXPECT_TRUE(nux::CanShowSigninModuleForTesting(*policy_map));

  policy_map = MakePolicy(policy::key::kBrowserSignin,
                          static_cast<int>(policy::BrowserSigninMode::kForced));
  EXPECT_TRUE(nux::CanShowSigninModuleForTesting(*policy_map));

  policy_map =
      MakePolicy(policy::key::kBrowserSignin,
                 static_cast<int>(policy::BrowserSigninMode::kDisabled));
  EXPECT_FALSE(nux::CanShowSigninModuleForTesting(*policy_map));
}
