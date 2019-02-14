// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/persistent_enrollment_scheduler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/fake_cryptauth_enrollment_scheduler.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kFakePolicyName[] = "fake-policy-name";
int kFakePolicyVersion = 100;
constexpr base::TimeDelta kFakeRefreshPeriod = base::TimeDelta::FromDays(100);
constexpr base::TimeDelta kFakeRetryPeriod = base::TimeDelta::FromHours(100);
const int kFakeMaxImmediateRetries = 2;

// The time set on the scheduler's clock during set-up.
const base::Time kFakeTimeNow = base::Time::FromDoubleT(1600600000);
const base::Time kFakeTimeLaterBeforeRetryPeriod =
    kFakeTimeNow + kFakeRetryPeriod - base::TimeDelta::FromHours(1);
const base::Time kFakeTimeLaterAfterRetryPeriod =
    kFakeTimeNow + kFakeRefreshPeriod + base::TimeDelta::FromHours(1);
const base::Time kFakeTimeLaterAfterRefreshPeriod =
    kFakeTimeNow + kFakeRefreshPeriod + base::TimeDelta::FromDays(1);

// Serializes and base64 encodes the input ClientDirective.
// Copied from cryptauth_enrollment_scheduler_impl.cc.
std::string ClientDirectiveToPrefString(
    const cryptauthv2::ClientDirective& client_directive) {
  std::string encoded_serialized_client_directive;
  base::Base64Encode(client_directive.SerializeAsString(),
                     &encoded_serialized_client_directive);

  return encoded_serialized_client_directive;
}

}  // namespace

class DeviceSyncPersistentEnrollmentSchedulerTest : public testing::Test {
 protected:
  DeviceSyncPersistentEnrollmentSchedulerTest() {
    fake_client_directive_.mutable_policy_reference()->set_name(
        kFakePolicyName);
    fake_client_directive_.mutable_policy_reference()->set_version(
        kFakePolicyVersion);
    fake_client_directive_.set_checkin_delay_millis(
        kFakeRefreshPeriod.InMilliseconds());
    fake_client_directive_.set_retry_period_millis(
        kFakeRetryPeriod.InMilliseconds());
    fake_client_directive_.set_retry_attempts(kFakeMaxImmediateRetries);
  };

  ~DeviceSyncPersistentEnrollmentSchedulerTest() override = default;

  void SetUp() override {
    PersistentEnrollmentScheduler::RegisterPrefs(pref_service_.registry());
  }

  void CreateScheduler() {
    if (scheduler_)
      return;

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    scheduler_ = PersistentEnrollmentScheduler::Factory::Get()->BuildInstance(
        &fake_delegate_, &pref_service_, &test_clock_, std::move(mock_timer),
        test_task_runner);

    test_task_runner->RunUntilIdle();
  }

  void VerifyReceivedPolicyReference(
      size_t num_expected_received_policy_references,
      const base::Optional<cryptauthv2::PolicyReference>&
          last_expected_received_policy_reference) {
    EXPECT_EQ(
        num_expected_received_policy_references,
        fake_delegate_.policy_references_from_enrollment_requests().size());

    if (fake_delegate_.policy_references_from_enrollment_requests().empty())
      return;

    EXPECT_EQ(last_expected_received_policy_reference.has_value(),
              fake_delegate_.policy_references_from_enrollment_requests()
                  .back()
                  .has_value());

    if (fake_delegate_.policy_references_from_enrollment_requests()
            .back()
            .has_value() &&
        last_expected_received_policy_reference.has_value()) {
      EXPECT_EQ(last_expected_received_policy_reference->SerializeAsString(),
                fake_delegate_.policy_references_from_enrollment_requests()
                    .back()
                    ->SerializeAsString());
    }
  }

  void VerifyLastEnrollmentAttemptTimePref(const base::Time& expected_time) {
    EXPECT_EQ(
        pref_service_.GetTime(
            prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime),
        expected_time);
  }

  FakeCryptAuthEnrollmentSchedulerDelegate* delegate() {
    return &fake_delegate_;
  }

  PrefService* pref_service() { return &pref_service_; }

  base::SimpleTestClock* clock() { return &test_clock_; }

  base::MockOneShotTimer* timer() { return mock_timer_; }

  CryptAuthEnrollmentScheduler* scheduler() { return scheduler_.get(); }

  const cryptauthv2::ClientDirective& fake_client_directive() {
    return fake_client_directive_;
  }

 private:
  FakeCryptAuthEnrollmentSchedulerDelegate fake_delegate_;
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  base::MockOneShotTimer* mock_timer_;

  cryptauthv2::ClientDirective fake_client_directive_;

  std::unique_ptr<CryptAuthEnrollmentScheduler> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncPersistentEnrollmentSchedulerTest);
};

TEST_F(DeviceSyncPersistentEnrollmentSchedulerTest,
       HandleSuccessfulEnrollmentResult) {
  clock()->SetNow(kFakeTimeNow);
  CreateScheduler();

  EXPECT_TRUE(timer()->IsRunning());
  EXPECT_FALSE(scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_TRUE(scheduler()->GetRefreshPeriod() >
              base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            scheduler()->GetTimeToNextEnrollmentRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_EQ(0u, scheduler()->GetNumConsecutiveFailures());

  timer()->Fire();

  EXPECT_TRUE(scheduler()->IsWaitingForEnrollmentResult());
  VerifyReceivedPolicyReference(1, base::nullopt);

  CryptAuthEnrollmentResult result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNewKeysEnrolled,
      fake_client_directive());
  scheduler()->HandleEnrollmentResult(result);

  EXPECT_TRUE(timer()->IsRunning());
  ASSERT_TRUE(scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(kFakeTimeNow, scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                fake_client_directive().checkin_delay_millis()),
            scheduler()->GetRefreshPeriod());
  EXPECT_EQ(scheduler()->GetRefreshPeriod(),
            scheduler()->GetTimeToNextEnrollmentRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_EQ(0u, scheduler()->GetNumConsecutiveFailures());
  VerifyLastEnrollmentAttemptTimePref(kFakeTimeNow);

  timer()->Fire();

  VerifyReceivedPolicyReference(2, fake_client_directive().policy_reference());
}

TEST_F(DeviceSyncPersistentEnrollmentSchedulerTest,
       NotDueForRefresh_RequestImmediateEnrollment) {
  pref_service()->SetString(
      prefs::kCryptAuthEnrollmentSchedulerClientDirective,
      ClientDirectiveToPrefString(fake_client_directive()));
  pref_service()->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime,
      kFakeTimeNow);
  pref_service()->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime,
      kFakeTimeNow);
  pref_service()->SetUint64(
      prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures, 0);

  clock()->SetNow(kFakeTimeLaterBeforeRetryPeriod);
  base::TimeDelta fake_elapsed_time =
      kFakeTimeLaterBeforeRetryPeriod - kFakeTimeNow;

  CreateScheduler();

  ASSERT_TRUE(scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(kFakeTimeNow, scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                fake_client_directive().checkin_delay_millis()),
            scheduler()->GetRefreshPeriod());
  EXPECT_EQ(scheduler()->GetRefreshPeriod() - fake_elapsed_time,
            scheduler()->GetTimeToNextEnrollmentRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForEnrollmentResult());

  scheduler()->RequestEnrollmentNow();
  EXPECT_TRUE(scheduler()->IsWaitingForEnrollmentResult());
  EXPECT_EQ(1u,
            delegate()->policy_references_from_enrollment_requests().size());
}

TEST_F(DeviceSyncPersistentEnrollmentSchedulerTest,
       DueForRefreshBeforeConstructed) {
  pref_service()->SetString(
      prefs::kCryptAuthEnrollmentSchedulerClientDirective,
      ClientDirectiveToPrefString(fake_client_directive()));
  pref_service()->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime,
      kFakeTimeNow);
  pref_service()->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime,
      kFakeTimeNow);
  pref_service()->SetUint64(
      prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures, 0);

  clock()->SetNow(kFakeTimeLaterAfterRefreshPeriod);

  CreateScheduler();

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            scheduler()->GetTimeToNextEnrollmentRequest());
}

TEST_F(DeviceSyncPersistentEnrollmentSchedulerTest, HandleFailures) {
  clock()->SetNow(kFakeTimeNow);
  CreateScheduler();

  // Expect all failed attempts to be retried immediately until the retry
  // attempt limit from the ClientDirective is exceeded.
  size_t expected_failure_count = 0;
  while (expected_failure_count <
         static_cast<size_t>(fake_client_directive().retry_attempts())) {
    timer()->Fire();
    CryptAuthEnrollmentResult result(
        CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
        fake_client_directive());
    scheduler()->HandleEnrollmentResult(result);
    ++expected_failure_count;

    EXPECT_EQ(expected_failure_count, scheduler()->GetNumConsecutiveFailures());
    ASSERT_FALSE(scheduler()->GetLastSuccessfulEnrollmentTime());
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                  fake_client_directive().checkin_delay_millis()),
              scheduler()->GetRefreshPeriod());
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
              scheduler()->GetTimeToNextEnrollmentRequest());
    VerifyLastEnrollmentAttemptTimePref(kFakeTimeNow);
  }

  // Since all immediate retry attempts were expended above, expect a failed
  // attempt to be retried after the ClientDirective's retry period.
  timer()->Fire();
  CryptAuthEnrollmentResult result(
      CryptAuthEnrollmentResult::ResultCode::kErrorCryptAuthServerOverloaded,
      fake_client_directive());
  scheduler()->HandleEnrollmentResult(result);

  EXPECT_EQ(static_cast<size_t>(fake_client_directive().retry_attempts() + 1),
            scheduler()->GetNumConsecutiveFailures());
  EXPECT_TRUE(timer()->IsRunning());
  ASSERT_FALSE(scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                fake_client_directive().checkin_delay_millis()),
            scheduler()->GetRefreshPeriod());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                fake_client_directive().retry_period_millis()),
            scheduler()->GetTimeToNextEnrollmentRequest());
  VerifyLastEnrollmentAttemptTimePref(kFakeTimeNow);

  clock()->SetNow(kFakeTimeLaterAfterRetryPeriod);

  // Expect a successful attempt to reset the failure count and adjust the
  // enrollment schedule to use the refresh period specified in the
  // ClientDirective.
  timer()->Fire();
  CryptAuthEnrollmentResult success_result(
      CryptAuthEnrollmentResult::ResultCode::kSuccessNoNewKeysNeeded,
      fake_client_directive());
  scheduler()->HandleEnrollmentResult(success_result);

  EXPECT_EQ(0u, scheduler()->GetNumConsecutiveFailures());
  EXPECT_TRUE(timer()->IsRunning());
  ASSERT_TRUE(scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(kFakeTimeLaterAfterRetryPeriod,
            scheduler()->GetLastSuccessfulEnrollmentTime());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(
                fake_client_directive().checkin_delay_millis()),
            scheduler()->GetRefreshPeriod());
  EXPECT_EQ(scheduler()->GetRefreshPeriod(),
            scheduler()->GetTimeToNextEnrollmentRequest());
  VerifyLastEnrollmentAttemptTimePref(kFakeTimeLaterAfterRetryPeriod);
}

TEST_F(DeviceSyncPersistentEnrollmentSchedulerTest, HandlePersistedFailures) {
  // Seed the preferences to simulate the previous scheduler using all of its
  // immediate retry attempts and making 10 periodic retry attempts.
  pref_service()->SetString(
      prefs::kCryptAuthEnrollmentSchedulerClientDirective,
      ClientDirectiveToPrefString(fake_client_directive()));
  pref_service()->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastSuccessfulEnrollmentTime,
      kFakeTimeNow);
  pref_service()->SetTime(
      prefs::kCryptAuthEnrollmentSchedulerLastEnrollmentAttemptTime,
      kFakeTimeLaterBeforeRetryPeriod);
  pref_service()->SetUint64(
      prefs::kCryptAuthEnrollmentSchedulerNumConsecutiveFailures,
      fake_client_directive().retry_attempts() + 10);

  clock()->SetNow(kFakeTimeLaterBeforeRetryPeriod);

  // On construction, if the persisted failure count is greater than 0, expect
  // that the scheduler resets the failure count to 1.
  CreateScheduler();

  EXPECT_EQ(1u, scheduler()->GetNumConsecutiveFailures());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            scheduler()->GetTimeToNextEnrollmentRequest());
  VerifyLastEnrollmentAttemptTimePref(kFakeTimeLaterBeforeRetryPeriod);
}

}  // namespace device_sync

}  // namespace chromeos
