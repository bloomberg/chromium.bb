// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_extension_policy_migrator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/hashed_extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";

const char kOldPolicy1[] = "OldPolicyOne";
const char kOldPolicy2[] = "OldPolicyTwo";
const char kOldPolicy3[] = "OldPolicyThree";
const char kOldPolicy4[] = "OldPolicyFour";
const char kNewPolicy1[] = "NewPolicyOne";
const char kNewPolicy2[] = "NewPolicyTwo";
const char kNewPolicy3[] = "NewPolicyThree";

const int kOldValue1 = 111;
const int kOldValue2 = 222;
const int kOldValue3 = 333;
const int kOldValue4 = 444;
const int kNewValue3 = 999;

const ExtensionPolicyMigrator::Migration kMigrations[] = {
    {kOldPolicy1, kNewPolicy1},
    {kOldPolicy2, kNewPolicy2},
    {kOldPolicy3, kNewPolicy3},
};

void SetPolicy(PolicyMap* policy,
               const char* policy_name,
               std::unique_ptr<base::Value> value) {
  policy->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

class TestingPolicyMigrator : public ChromeExtensionPolicyMigrator {
 public:
  void Migrate(PolicyBundle* bundle) override {
    CopyPoliciesIfUnset(bundle,
                        extensions::HashedExtensionId(kExtensionId).value(),
                        kMigrations);
  }
};

}  // namespace

TEST(ChromeExtensionPolicyMigratorTest, CopyPoliciesIfUnset) {
  PolicyBundle bundle;

  PolicyMap& chrome_map = bundle.Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, /* component_id */ std::string()));
  SetPolicy(&chrome_map, kNewPolicy3,
            std::make_unique<base::Value>(kNewValue3));

  PolicyMap& extension_map =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, kExtensionId));
  SetPolicy(&extension_map, kOldPolicy1,
            std::make_unique<base::Value>(kOldValue1));
  SetPolicy(&extension_map, kOldPolicy2,
            std::make_unique<base::Value>(kOldValue2));
  SetPolicy(&extension_map, kOldPolicy3,
            std::make_unique<base::Value>(kOldValue3));
  SetPolicy(&extension_map, kOldPolicy4,
            std::make_unique<base::Value>(kOldValue4));

  TestingPolicyMigrator().Migrate(&bundle);

  // Policies in kMigrations should be renamed + copied into the Chrome domain.
  EXPECT_EQ(3u, chrome_map.size());
  ASSERT_TRUE(chrome_map.GetValue(kNewPolicy1));
  EXPECT_EQ(base::Value(kOldValue1), *chrome_map.GetValue(kNewPolicy1));
  ASSERT_TRUE(chrome_map.GetValue(kNewPolicy2));
  EXPECT_EQ(base::Value(kOldValue2), *chrome_map.GetValue(kNewPolicy2));
  // kNewPolicy3 is already set, and should not be overwritten.
  ASSERT_TRUE(chrome_map.GetValue(kNewPolicy3));
  EXPECT_EQ(base::Value(kNewValue3), *chrome_map.GetValue(kNewPolicy3));
}

}  // namespace policy
