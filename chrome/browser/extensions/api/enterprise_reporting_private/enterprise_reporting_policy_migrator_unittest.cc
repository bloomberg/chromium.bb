// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_policy_migrator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace enterprise_reporting {

namespace {

const char kStableExtensionId[] = "emahakmocgideepebncgnmlmliepgpgb";
const char kBetaExtensionId[] = "kigjhoekjcpdfjpimbdjegmgecmlicaf";

const policy::ExtensionPolicyMigrator::Migration kMigrations[] = {
    {"report_version_data", policy::key::kReportVersionData},
    {"report_policy_data", policy::key::kReportPolicyData},
    {"report_machine_id_data", policy::key::kReportMachineIDData},
    {"report_user_id_data", policy::key::kReportUserIDData},
};

void SetPolicy(policy::PolicyMap* policy,
               const char* policy_name,
               std::unique_ptr<base::Value> value) {
  policy->Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
              policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
              std::move(value), nullptr);
}

}  // namespace

TEST(EnterpriseReportingPolicyMigratorTest, Migrate) {
  policy::PolicyBundle bundle;

  policy::PolicyMap& stable_extension_map = bundle.Get(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_EXTENSIONS, kStableExtensionId));
  SetPolicy(&stable_extension_map, kMigrations[0].old_name,
            std::make_unique<base::Value>(false));
  SetPolicy(&stable_extension_map, kMigrations[1].old_name,
            std::make_unique<base::Value>(false));
  SetPolicy(&stable_extension_map, kMigrations[2].old_name,
            std::make_unique<base::Value>(false));

  policy::PolicyMap& beta_extension_map = bundle.Get(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_EXTENSIONS, kBetaExtensionId));
  SetPolicy(&beta_extension_map, kMigrations[2].old_name,
            std::make_unique<base::Value>(true));
  SetPolicy(&beta_extension_map, kMigrations[3].old_name,
            std::make_unique<base::Value>(true));

  EnterpriseReportingPolicyMigrator().Migrate(&bundle);

  policy::PolicyMap& chrome_map = bundle.Get(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_CHROME, /* component_id */ std::string()));

  EXPECT_EQ(4u, chrome_map.size());
  EXPECT_EQ(base::Value(false), *chrome_map.GetValue(kMigrations[0].new_name));
  EXPECT_EQ(base::Value(false), *chrome_map.GetValue(kMigrations[1].new_name));
  // Stable takes priority over Beta, when the policy is set.
  EXPECT_EQ(base::Value(false), *chrome_map.GetValue(kMigrations[2].new_name));
  EXPECT_EQ(base::Value(true), *chrome_map.GetValue(kMigrations[3].new_name));
}

}  // namespace enterprise_reporting
}  // namespace extensions
