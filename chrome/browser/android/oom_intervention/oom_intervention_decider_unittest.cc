// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"

#include "base/strings/string_number_conversions.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class OomInterventionDeciderTest : public testing::Test {
 protected:
  OomInterventionDeciderTest() {
    OomInterventionDecider::RegisterProfilePrefs(prefs_.registry());
  }

  void SetUp() override { decider_.reset(new OomInterventionDecider(&prefs_)); }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<OomInterventionDecider> decider_;
};

TEST_F(OomInterventionDeciderTest, OptOutSingleHost) {
  std::string host = "www.example.com";

  EXPECT_TRUE(decider_->CanTriggerIntervention(host));
  EXPECT_FALSE(decider_->IsOptedOut(host));

  decider_->OnInterventionDeclined(host);
  EXPECT_FALSE(decider_->CanTriggerIntervention(host));
  EXPECT_FALSE(decider_->IsOptedOut(host));

  decider_->OnOomDetected(host);
  EXPECT_TRUE(decider_->CanTriggerIntervention(host));
  EXPECT_FALSE(decider_->IsOptedOut(host));

  decider_->OnInterventionDeclined(host);
  EXPECT_FALSE(decider_->CanTriggerIntervention(host));
  EXPECT_TRUE(decider_->IsOptedOut(host));
}

TEST_F(OomInterventionDeciderTest, ParmanentlyOptOut) {
  std::string not_declined_host = "not_declined_host";
  EXPECT_TRUE(decider_->CanTriggerIntervention(not_declined_host));

  // Put sufficient number of hosts into the blacklist.
  for (size_t i = 0; i < OomInterventionDecider::kMaxBlacklistSize; ++i) {
    std::string declined_host = "declined_host" + base::NumberToString(i);
    decider_->OnInterventionDeclined(declined_host);
    decider_->OnOomDetected(declined_host);
    decider_->OnInterventionDeclined(declined_host);
  }

  EXPECT_FALSE(decider_->CanTriggerIntervention(not_declined_host));
  EXPECT_TRUE(decider_->IsOptedOut(not_declined_host));
}
