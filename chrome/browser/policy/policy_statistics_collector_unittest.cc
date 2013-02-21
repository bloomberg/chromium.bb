// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/policy/mock_policy_service.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_statistics_collector.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using testing::_;
using testing::Lt;
using testing::Return;
using testing::ReturnRef;

// Arbitrary policy names used for testing.
const char* const kTestPolicy1 = key::kHomepageIsNewTabPage;
const char* const kTestPolicy2 = key::kInstantEnabled;

class TestPolicyStatisticsCollector : public PolicyStatisticsCollector {
 public:
  TestPolicyStatisticsCollector(
      PolicyService* policy_service,
      PrefService* prefs,
      const scoped_refptr<base::TaskRunner>& task_runner)
      : PolicyStatisticsCollector(policy_service, prefs, task_runner) {
  }

  MOCK_METHOD1(RecordPolicyUse, void(int));
};

}  // namespace

class PolicyStatisticsCollectorTest : public testing::Test {
 protected:
  PolicyStatisticsCollectorTest()
      : update_delay_(base::TimeDelta::FromMilliseconds(
            PolicyStatisticsCollector::kStatisticsUpdateRate)),
        test_policy_id1_(-1),
        test_policy_id2_(-1),
        task_runner_(new base::TestSimpleTaskRunner()) {
  }

  virtual void SetUp() OVERRIDE {
    chrome::RegisterLocalState(prefs_.registry());

    // Find ids for kTestPolicy1 and kTestPolicy2.
    const policy::PolicyDefinitionList* policy_list =
        policy::GetChromePolicyDefinitionList();
    for (const policy::PolicyDefinitionList::Entry* policy = policy_list->begin;
         policy != policy_list->end; ++policy) {
      if (strcmp(policy->name, kTestPolicy1) == 0)
        test_policy_id1_ = policy->id;
      else if (strcmp(policy->name, kTestPolicy2) == 0)
        test_policy_id2_ = policy->id;
    }
    ASSERT_TRUE(test_policy_id1_ != -1);
    ASSERT_TRUE(test_policy_id2_ != -1);

    // Set up default function behaviour.
    EXPECT_CALL(policy_service_,
                GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                            std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));

    // Arbitrary negative value (so it'll be different from |update_delay_|).
    last_delay_ = base::TimeDelta::FromDays(-1);
    policy_map_.Clear();
    policy_statistics_collector_.reset(new TestPolicyStatisticsCollector(
        &policy_service_,
        &prefs_,
        task_runner_));
  }

  void SetPolicy(const std::string& name) {
    policy_map_.Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    base::Value::CreateBooleanValue(true));
  }

  base::TimeDelta GetFirstDelay() const {
    if (task_runner_->GetPendingTasks().empty()) {
      ADD_FAILURE();
      return base::TimeDelta();
    }
    return task_runner_->GetPendingTasks().front().delay;
  }

  const base::TimeDelta update_delay_;

  int test_policy_id1_;
  int test_policy_id2_;

  base::TimeDelta last_delay_;

  TestingPrefServiceSimple prefs_;
  MockPolicyService policy_service_;
  PolicyMap policy_map_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<TestPolicyStatisticsCollector> policy_statistics_collector_;
};

TEST_F(PolicyStatisticsCollectorTest, CollectPending) {
  SetPolicy(kTestPolicy1);

  prefs_.SetInt64(prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_.get(),
              RecordPolicyUse(test_policy_id1_));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  EXPECT_EQ(update_delay_, GetFirstDelay());
}

TEST_F(PolicyStatisticsCollectorTest, CollectPendingVeryOld) {
  SetPolicy(kTestPolicy1);

  // Must not be 0.0 (read comment for Time::FromDoubleT).
  prefs_.SetInt64(prefs::kLastPolicyStatisticsUpdate,
                  base::Time::FromDoubleT(1.0).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_.get(),
              RecordPolicyUse(test_policy_id1_));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  EXPECT_EQ(update_delay_, GetFirstDelay());
}

TEST_F(PolicyStatisticsCollectorTest, CollectLater) {
  SetPolicy(kTestPolicy1);

  prefs_.SetInt64(prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_ / 2).ToInternalValue());

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  EXPECT_LT(GetFirstDelay(), update_delay_);
}

TEST_F(PolicyStatisticsCollectorTest, MultiplePolicies) {
  SetPolicy(kTestPolicy1);
  SetPolicy(kTestPolicy2);

  prefs_.SetInt64(prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_.get(),
              RecordPolicyUse(test_policy_id1_));
  EXPECT_CALL(*policy_statistics_collector_.get(),
              RecordPolicyUse(test_policy_id2_));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
}

}  // namespace policy
