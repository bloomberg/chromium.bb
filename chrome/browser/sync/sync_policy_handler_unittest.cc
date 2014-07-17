// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/sync/sync_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/sync_driver/pref_names.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

// Test cases for the Sync policy setting.
class SyncPolicyHandlerTest : public testing::Test {};

TEST_F(SyncPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(prefs.GetValue(sync_driver::prefs::kSyncManaged, NULL));
}

TEST_F(SyncPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kSyncDisabled,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             new base::FundamentalValue(false),
             NULL);
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Sync should not set the pref.
  EXPECT_FALSE(prefs.GetValue(sync_driver::prefs::kSyncManaged, NULL));
}

TEST_F(SyncPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kSyncDisabled,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             new base::FundamentalValue(true),
             NULL);
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);

  // Sync should be flagged as managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(prefs.GetValue(sync_driver::prefs::kSyncManaged, &value));
  ASSERT_TRUE(value);
  bool sync_managed = false;
  bool result = value->GetAsBoolean(&sync_managed);
  ASSERT_TRUE(result);
  EXPECT_TRUE(sync_managed);
}

}  // namespace browser_sync
