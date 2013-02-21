// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_core.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/mock_cloud_policy_client.h"
#include "chrome/browser/policy/mock_cloud_policy_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class CloudPolicyCoreTest : public testing::Test {
 protected:
  CloudPolicyCoreTest()
      : core_(PolicyNamespaceKey(dm_protocol::kChromeUserPolicyType,
                                 std::string()),
              &store_) {
    chrome::RegisterLocalState(prefs_.registry());
  }

  MessageLoop loop_;

  TestingPrefServiceSimple prefs_;
  MockCloudPolicyStore store_;
  CloudPolicyCore core_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCoreTest);
};

TEST_F(CloudPolicyCoreTest, ConnectAndDisconnect) {
  EXPECT_TRUE(core_.store());
  EXPECT_FALSE(core_.client());
  EXPECT_FALSE(core_.service());
  EXPECT_FALSE(core_.refresh_scheduler());

  // Connect() brings up client and service.
  core_.Connect(scoped_ptr<CloudPolicyClient>(new MockCloudPolicyClient()));
  EXPECT_TRUE(core_.client());
  EXPECT_TRUE(core_.service());
  EXPECT_FALSE(core_.refresh_scheduler());

  // Disconnect() goes back to no client and service.
  core_.Disconnect();
  EXPECT_FALSE(core_.client());
  EXPECT_FALSE(core_.service());
  EXPECT_FALSE(core_.refresh_scheduler());

  // Calling Disconnect() twice doesn't do bad things.
  core_.Disconnect();
  EXPECT_FALSE(core_.client());
  EXPECT_FALSE(core_.service());
  EXPECT_FALSE(core_.refresh_scheduler());
}

TEST_F(CloudPolicyCoreTest, RefreshScheduler) {
  EXPECT_FALSE(core_.refresh_scheduler());
  core_.Connect(scoped_ptr<CloudPolicyClient>(new MockCloudPolicyClient()));
  core_.StartRefreshScheduler();
  ASSERT_TRUE(core_.refresh_scheduler());

  int default_refresh_delay = core_.refresh_scheduler()->refresh_delay();

  const int kRefreshRate = 1000 * 60 * 60;
  prefs_.SetInteger(prefs::kUserPolicyRefreshRate, kRefreshRate);
  core_.TrackRefreshDelayPref(&prefs_, prefs::kUserPolicyRefreshRate);
  EXPECT_EQ(kRefreshRate, core_.refresh_scheduler()->refresh_delay());

  prefs_.ClearPref(prefs::kUserPolicyRefreshRate);
  EXPECT_EQ(default_refresh_delay, core_.refresh_scheduler()->refresh_delay());
}

}  // namespace policy
