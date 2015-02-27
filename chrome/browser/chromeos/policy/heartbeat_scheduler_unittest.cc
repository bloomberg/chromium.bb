// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/heartbeat_scheduler.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace {
const char* const kFakeEnrollmentDomain = "example.com";
const char* const kFakeDeviceId = "fake_device_id";
const char* const kHeartbeatGCMAppID = "com.google.chromeos.monitoring";

class MockGCMDriver : public testing::StrictMock<gcm::FakeGCMDriver> {
 public:
  MockGCMDriver() {
  }

  ~MockGCMDriver() override {
  }

  MOCK_METHOD2(RegisterImpl,
               void(const std::string&, const std::vector<std::string>&));
  MOCK_METHOD3(SendImpl,
               void(const std::string&, const std::string&,
                    const gcm::GCMClient::OutgoingMessage& message));


  // Helper function to complete a registration previously started by
  // Register().
  void CompleteRegistration(const std::string& app_id,
                            gcm::GCMClient::Result result) {
    RegisterFinished(app_id, "registration_id", result);
  }

  // Helper function to complete a send operation previously started by
  // Send().
  void CompleteSend(const std::string& app_id,
                    const std::string& message_id,
                    gcm::GCMClient::Result result) {
    SendFinished(app_id, message_id, result);
  }
};

class HeartbeatSchedulerTest : public testing::Test {
 public:
  HeartbeatSchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        scheduler_(
            &gcm_driver_, kFakeEnrollmentDomain, kFakeDeviceId, task_runner_) {
  }

  void SetUp() override {
    // Swap out the DeviceSettingsProvider with our stub settings provider
    // so we can set values for the heartbeat frequency.
    chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
    device_settings_provider_ =
        cros_settings->GetProvider(chromeos::kReportDeviceVersionInfo);
    EXPECT_TRUE(device_settings_provider_);
    EXPECT_TRUE(
        cros_settings->RemoveSettingsProvider(device_settings_provider_));
    cros_settings->AddSettingsProvider(&stub_settings_provider_);
  }

  void TearDown() override {
    content::RunAllBlockingPoolTasksUntilIdle();
    // Restore the real DeviceSettingsProvider.
    chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
    EXPECT_TRUE(cros_settings->RemoveSettingsProvider(
        &stub_settings_provider_));
    cros_settings->AddSettingsProvider(device_settings_provider_);
  }

  void CheckPendingTaskDelay(base::Time last_heartbeat,
                             base::TimeDelta expected_delay) {
    EXPECT_FALSE(last_heartbeat.is_null());
    base::Time now = base::Time::NowFromSystemTime();
    EXPECT_GE(now, last_heartbeat);
    base::TimeDelta actual_delay = task_runner_->NextPendingTaskDelay();

    // NextPendingTaskDelay() returns the exact original delay value the task
    // was posted with. The heartbeat task would have been calculated to fire at
    // |last_heartbeat| + |expected_delay|, but we don't know the exact time
    // when the task was posted (if it was a couple of milliseconds after
    // |last_heartbeat|, then |actual_delay| would be a couple of milliseconds
    // smaller than |expected_delay|.
    //
    // We do know that the task was posted sometime between |last_heartbeat|
    // and |now|, so we know that 0 <= |expected_delay| - |actual_delay| <=
    // |now| - |last_heartbeat|.
    base::TimeDelta delta = expected_delay - actual_delay;
    EXPECT_LE(base::TimeDelta(), delta);
    EXPECT_GE(now - last_heartbeat, delta);
  }

  base::MessageLoop loop_;

  // Helpers used to mock out cros settings.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::CrosSettingsProvider* device_settings_provider_;
  chromeos::StubCrosSettingsProvider stub_settings_provider_;

  MockGCMDriver gcm_driver_;

  // TaskRunner used to run individual tests.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // The HeartbeatScheduler instance under test.
  policy::HeartbeatScheduler scheduler_;
};

TEST_F(HeartbeatSchedulerTest, Basic) {
  // Just makes sure we can spin up and shutdown the scheduler with
  // heartbeats disabled.
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, false);
  ASSERT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(HeartbeatSchedulerTest, PermanentlyFailedGCMRegistration) {
  // If heartbeats are enabled, we should register with GCMDriver.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::GCM_DISABLED);

  // There should be no heartbeat tasks pending, because registration failed.
  ASSERT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(HeartbeatSchedulerTest, TemporarilyFailedGCMRegistration) {
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SERVER_ERROR);
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  // Should have a pending task to try registering again.
  ASSERT_FALSE(task_runner_->GetPendingTasks().empty());
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  // Once we have successfully registered, we should send a heartbeat.
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, _));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  task_runner_->RunPendingTasks();
}

TEST_F(HeartbeatSchedulerTest, ChangeHeartbeatFrequency) {
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);

  EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());
  // Should have a heartbeat task posted with zero delay on startup.
  EXPECT_EQ(base::TimeDelta(), task_runner_->NextPendingTaskDelay());
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  const int new_delay = 1234*1000;  // 1234 seconds.
  chromeos::CrosSettings::Get()->SetInteger(chromeos::kHeartbeatFrequency,
                                            new_delay);
  // Now run pending heartbeat task, should send a heartbeat.
  gcm::GCMClient::OutgoingMessage message;
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, _))
      .WillOnce(SaveArg<2>(&message));
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());

  // Complete sending a message - we should queue up the next heartbeat
  // even if the previous attempt failed.
  gcm_driver_.CompleteSend(
      kHeartbeatGCMAppID, message.id, gcm::GCMClient::SERVER_ERROR);
  EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());
  CheckPendingTaskDelay(scheduler_.last_heartbeat(),
                        base::TimeDelta::FromMilliseconds(new_delay));
}

TEST_F(HeartbeatSchedulerTest, DisableHeartbeats) {
  // Makes sure that we can disable heartbeats on the fly.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm::GCMClient::OutgoingMessage message;
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, _))
      .WillOnce(SaveArg<2>(&message));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  // Should have a heartbeat task posted.
  EXPECT_EQ(1U, task_runner_->GetPendingTasks().size());
  task_runner_->RunPendingTasks();

  // Complete sending a message - we should queue up the next heartbeat.
  gcm_driver_.CompleteSend(
      kHeartbeatGCMAppID, message.id, gcm::GCMClient::SUCCESS);

  // Should have a new heartbeat task posted.
  ASSERT_EQ(1U, task_runner_->GetPendingTasks().size());
  CheckPendingTaskDelay(
      scheduler_.last_heartbeat(),
      base::TimeDelta::FromMilliseconds(
          policy::HeartbeatScheduler::kDefaultHeartbeatIntervalMs));
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  // Now disable heartbeats. Should get no more heartbeats sent.
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, false);
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(HeartbeatSchedulerTest, CheckMessageContents) {
  gcm::GCMClient::OutgoingMessage message;
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, _))
      .WillOnce(SaveArg<2>(&message));
  chromeos::CrosSettings::Get()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  task_runner_->RunPendingTasks();

  // Heartbeats should have a time-to-live equivalent to the heartbeat frequency
  // so we don't have more than one heartbeat queued at a time.
  EXPECT_EQ(policy::HeartbeatScheduler::kDefaultHeartbeatIntervalMs/1000,
            message.time_to_live);

  // Check the values in the message payload.
  EXPECT_EQ("hb", message.data["type"]);
  int64 timestamp;
  EXPECT_TRUE(base::StringToInt64(message.data["timestamp"], &timestamp));
  EXPECT_EQ(kFakeEnrollmentDomain, message.data["domain_name"]);
  EXPECT_EQ(kFakeDeviceId, message.data["device_id"]);
}

}  // namespace
