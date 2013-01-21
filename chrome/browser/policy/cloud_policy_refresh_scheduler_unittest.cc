// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/mock_cloud_policy_client.h"
#include "chrome/browser/policy/mock_cloud_policy_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace policy {

namespace {
const int64 kPolicyRefreshRate = 4 * 60 * 60 * 1000;
}  // namespace

class CloudPolicyRefreshSchedulerTest : public testing::Test {
 protected:
  CloudPolicyRefreshSchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        network_change_notifier_(net::NetworkChangeNotifier::CreateMock()) {}

  virtual void SetUp() OVERRIDE {
    client_.SetDMToken("token");

    // Set up the protobuf timestamp to be one minute in the past. Since the
    // protobuf field only has millisecond precision, we convert the actual
    // value back to get a millisecond-clamped time stamp for the checks below.
    store_.policy_.reset(new em::PolicyData());
    store_.policy_->set_timestamp(
        ((base::Time::NowFromSystemTime() - base::TimeDelta::FromMinutes(1)) -
         base::Time::UnixEpoch()).InMilliseconds());
    last_refresh_ =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(store_.policy_->timestamp());
  }

  CloudPolicyRefreshScheduler* CreateRefreshScheduler() {
    CloudPolicyRefreshScheduler* scheduler =
        new CloudPolicyRefreshScheduler(&client_, &store_, task_runner_);
    scheduler->SetRefreshDelay(kPolicyRefreshRate);
    return scheduler;
  }

  void NotifyIPAddressChanged() {
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    loop_.RunUntilIdle();
  }

  base::TimeDelta GetLastDelay() const {
    const std::deque<base::TestPendingTask>& pending_tasks =
        task_runner_->GetPendingTasks();
    return
        pending_tasks.empty() ? base::TimeDelta() : pending_tasks.back().delay;
  }

  void CheckTiming(int64 expected_delay_ms) const {
    EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
    base::Time now(base::Time::NowFromSystemTime());
    base::TimeDelta expected_delay(
        base::TimeDelta::FromMilliseconds(expected_delay_ms));
    EXPECT_GE(GetLastDelay(), expected_delay - (now - last_refresh_));
    EXPECT_LE(GetLastDelay(), expected_delay);
  }

  MessageLoop loop_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  // Base time for the refresh that the scheduler should be using.
  base::Time last_refresh_;
};

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshNoPolicy) {
  store_.policy_.reset();
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshUnmanaged) {
  store_.policy_->set_state(em::PolicyData::UNMANAGED);
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  CheckTiming(CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedNotYetFetched) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedAlreadyFetched) {
  last_refresh_ = base::Time::NowFromSystemTime();
  client_.SetPolicy(PolicyNamespaceKey(dm_protocol::kChromeUserPolicyType,
                                       std::string()),
      em::PolicyFetchResponse());
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  CheckTiming(kPolicyRefreshRate);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, Unregistered) {
  client_.SetDMToken("");
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  client_.NotifyPolicyFetched();
  client_.NotifyRegistrationStateChanged();
  client_.NotifyClientError();
  scheduler->SetRefreshDelay(12 * 60 * 60 * 1000);
  store_.NotifyStoreLoaded();
  store_.NotifyStoreError();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

class CloudPolicyRefreshSchedulerSteadyStateTest
    : public CloudPolicyRefreshSchedulerTest {
 protected:
  CloudPolicyRefreshSchedulerSteadyStateTest()
      : refresh_scheduler_(&client_, &store_, task_runner_) {}

  virtual void SetUp() OVERRIDE {
    refresh_scheduler_.SetRefreshDelay(kPolicyRefreshRate);
    CloudPolicyRefreshSchedulerTest::SetUp();
    last_refresh_ = base::Time::NowFromSystemTime();
    client_.NotifyPolicyFetched();
    CheckTiming(kPolicyRefreshRate);
  }

  CloudPolicyRefreshScheduler refresh_scheduler_;
};

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnPolicyFetched) {
  client_.NotifyPolicyFetched();
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnRegistrationStateChanged) {
  client_.SetDMToken("new_token");
  client_.NotifyRegistrationStateChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());

  task_runner_->ClearPendingTasks();
  client_.SetDMToken("");
  client_.NotifyRegistrationStateChanged();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreLoaded) {
  store_.NotifyStoreLoaded();
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreError) {
  task_runner_->ClearPendingTasks();
  store_.NotifyStoreError();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, RefreshDelayChange) {
  const int delay_short_ms = 5 * 60 * 1000;
  refresh_scheduler_.SetRefreshDelay(delay_short_ms);
  CheckTiming(CloudPolicyRefreshScheduler::kRefreshDelayMinMs);

  const int delay_ms = 12 * 60 * 60 * 1000;
  refresh_scheduler_.SetRefreshDelay(delay_ms);
  CheckTiming(delay_ms);

  const int delay_long_ms = 2 * 24 * 60 * 60 * 1000;
  refresh_scheduler_.SetRefreshDelay(delay_long_ms);
  CheckTiming(CloudPolicyRefreshScheduler::kRefreshDelayMaxMs);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnIPAddressChanged) {
  NotifyIPAddressChanged();
  CheckTiming(kPolicyRefreshRate);

  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  NotifyIPAddressChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
}

struct ClientErrorTestParam {
  DeviceManagementStatus client_error;
  int64 expected_delay_ms;
  int backoff_factor;
};

static const ClientErrorTestParam kClientErrorTestCases[] = {
  { DM_STATUS_REQUEST_INVALID,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_REQUEST_FAILED,
    CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2 },
  { DM_STATUS_TEMPORARY_UNAVAILABLE,
    CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2 },
  { DM_STATUS_HTTP_STATUS_ERROR,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_RESPONSE_DECODING_ERROR,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
    -1, 1 },
  { DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
    -1, 1 },
  { DM_STATUS_SERVICE_ACTIVATION_PENDING,
    kPolicyRefreshRate, 1 },
  { DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
    -1, 1 },
  { DM_STATUS_SERVICE_MISSING_LICENSES,
    -1, 1 },
  { DM_STATUS_SERVICE_DEVICE_ID_CONFLICT,
    -1, 1 },
  { DM_STATUS_SERVICE_POLICY_NOT_FOUND,
    kPolicyRefreshRate, 1 },
};

class CloudPolicyRefreshSchedulerClientErrorTest
    : public CloudPolicyRefreshSchedulerSteadyStateTest,
      public testing::WithParamInterface<ClientErrorTestParam> {
};

TEST_P(CloudPolicyRefreshSchedulerClientErrorTest, OnClientError) {
  client_.SetStatus(GetParam().client_error);
  task_runner_->ClearPendingTasks();

  // See whether the error triggers the right refresh delay.
  int64 expected_delay_ms = GetParam().expected_delay_ms;
  client_.NotifyClientError();
  if (expected_delay_ms >= 0) {
    CheckTiming(expected_delay_ms);

    // Check whether exponential backoff is working as expected and capped at
    // the regular refresh rate (if applicable).
    do {
      expected_delay_ms *= GetParam().backoff_factor;
      last_refresh_ = base::Time::NowFromSystemTime();
      client_.NotifyClientError();
      CheckTiming(std::max(std::min(expected_delay_ms, kPolicyRefreshRate),
                           GetParam().expected_delay_ms));
    } while (GetParam().backoff_factor > 1 &&
             expected_delay_ms <= kPolicyRefreshRate);
  } else {
    EXPECT_EQ(base::TimeDelta(), GetLastDelay());
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  }
}

INSTANTIATE_TEST_CASE_P(CloudPolicyRefreshSchedulerClientErrorTest,
                        CloudPolicyRefreshSchedulerClientErrorTest,
                        testing::ValuesIn(kClientErrorTestCases));

}  // namespace policy
