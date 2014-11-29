// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

#include "policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

class CloudPolicyConstantsTest : public testing::Test {
 public:
  em::PolicyData policy_data_;
};

TEST_F(CloudPolicyConstantsTest, GetManagementModeForLegacyPolicyData) {
  // Legacy PolicyData does not have |management_mode|.
  policy_data_.clear_management_mode();

  // PolicyData with no |request_token| is locally owned.
  policy_data_.clear_request_token();
  EXPECT_EQ(MANAGEMENT_MODE_LOCAL_OWNER, GetManagementMode(policy_data_));

  // PolicyData with |request_token| is enterprise-managed.
  policy_data_.set_request_token("fake_request_token");
  EXPECT_EQ(MANAGEMENT_MODE_ENTERPRISE_MANAGED,
            GetManagementMode(policy_data_));
}

TEST_F(CloudPolicyConstantsTest,
       GetManagementModeForPolicyDataWithManagementMode) {
  policy_data_.set_request_token("fake_request_token");

  policy_data_.set_management_mode(em::PolicyData::CONSUMER_MANAGED);
  EXPECT_EQ(MANAGEMENT_MODE_CONSUMER_MANAGED, GetManagementMode(policy_data_));

  policy_data_.set_management_mode(em::PolicyData::ENTERPRISE_MANAGED);
  EXPECT_EQ(MANAGEMENT_MODE_ENTERPRISE_MANAGED,
            GetManagementMode(policy_data_));

  policy_data_.set_management_mode(em::PolicyData::LOCAL_OWNER);
  EXPECT_EQ(MANAGEMENT_MODE_LOCAL_OWNER, GetManagementMode(policy_data_));
}

}  // namespace policy
