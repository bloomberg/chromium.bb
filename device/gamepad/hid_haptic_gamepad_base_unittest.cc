// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad_base.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Describes a hypothetical HID haptic gamepad with two 16-bit vibration
// magnitude fields.
constexpr uint8_t kReportId = 0x42;
constexpr size_t kReportLength = 5;
constexpr HidHapticGamepadBase::HapticReportData kHapticReportData = {
    0x1234, 0xabcd, kReportId, kReportLength, 1, 3, 16, 0, 0xffff};
// The expected "stop vibration" report bytes.
constexpr uint8_t kStopVibrationData[] = {kReportId, 0x00, 0x00, 0x00, 0x00};
// The expected "start vibration" report bytes.
constexpr uint8_t kStartVibrationData[] = {kReportId, 0xff, 0xff, 0xff, 0x7f};

// An implementation of HidHapticGamepadBase that records its output reports.
class FakeHidHapticGamepad : public HidHapticGamepadBase {
 public:
  FakeHidHapticGamepad(const HidHapticGamepadBase::HapticReportData& data)
      : HidHapticGamepadBase(data) {}
  ~FakeHidHapticGamepad() override = default;

  base::TimeDelta TaskDelayFromMilliseconds(double delay_millis) override {
    // Remove delays for testing.
    return base::TimeDelta();
  }

  size_t WriteOutputReport(void* report, size_t report_length) override {
    std::vector<uint8_t> report_bytes(report_length);
    const uint8_t* report_begin = reinterpret_cast<uint8_t*>(report);
    const uint8_t* report_end = report_begin + report_length;
    std::copy(report_begin, report_end, report_bytes.begin());
    output_reports_.push_back(std::move(report_bytes));
    return report_length;
  }

  std::vector<std::vector<uint8_t>> output_reports_;
};

// Main test fixture
class HidHapticGamepadBaseTest : public testing::Test {
 public:
  HidHapticGamepadBaseTest()
      : start_vibration_output_report_(kStartVibrationData,
                                       kStartVibrationData + kReportLength),
        stop_vibration_output_report_(kStopVibrationData,
                                      kStopVibrationData + kReportLength),
        first_callback_count_(0),
        second_callback_count_(0),
        first_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        second_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        gamepad_(std::make_unique<FakeHidHapticGamepad>(kHapticReportData)),
        task_runner_(new base::TestSimpleTaskRunner) {}

  void TearDown() override { gamepad_->Shutdown(); }

  void PostPlayEffect(
      mojom::GamepadHapticEffectType type,
      double duration,
      double start_delay,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeHidHapticGamepad::PlayEffect,
                       base::Unretained(gamepad_.get()), type,
                       mojom::GamepadEffectParameters::New(
                           duration, start_delay, 1.0, 0.5),
                       std::move(callback)),
        base::TimeDelta());
  }

  void PostResetVibration(
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeHidHapticGamepad::ResetVibration,
                       base::Unretained(gamepad_.get()), std::move(callback)),
        base::TimeDelta());
  }

  void FirstCallback(mojom::GamepadHapticsResult result) {
    first_callback_count_++;
    first_callback_result_ = result;
  }

  void SecondCallback(mojom::GamepadHapticsResult result) {
    second_callback_count_++;
    second_callback_result_ = result;
  }

  const std::vector<uint8_t> start_vibration_output_report_;
  const std::vector<uint8_t> stop_vibration_output_report_;
  int first_callback_count_;
  int second_callback_count_;
  mojom::GamepadHapticsResult first_callback_result_;
  mojom::GamepadHapticsResult second_callback_result_;
  std::unique_ptr<FakeHidHapticGamepad> gamepad_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(HidHapticGamepadBaseTest);
};

TEST_F(HidHapticGamepadBaseTest, PlayEffectTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      0.0,
      base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                     base::Unretained(this)));

  // Run the queued task, but stop before executing any new tasks queued by that
  // task. This should pause before sending any output reports.
  task_runner_->RunPendingTasks();

  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_TRUE(task_runner_->HasPendingTask());

  // Run the next task, but pause before completing the effect.
  task_runner_->RunPendingTasks();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_TRUE(task_runner_->HasPendingTask());

  // Complete the effect and issue the callback. After this, there should be no
  // more pending tasks.
  task_runner_->RunPendingTasks();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_,
                                   stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(HidHapticGamepadBaseTest, ResetVibrationTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostResetVibration(base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                    base::Unretained(this)));

  task_runner_->RunUntilIdle();

  // ResetVibration should return a "complete" result without sending any
  // reports.
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
}

TEST_F(HidHapticGamepadBaseTest, UnsupportedEffectTypeTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  mojom::GamepadHapticEffectType unsupported_effect_type =
      static_cast<mojom::GamepadHapticEffectType>(123);
  PostPlayEffect(unsupported_effect_type, 1.0, 0.0,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  task_runner_->RunUntilIdle();

  // An unsupported effect should return a "not-supported" result without
  // sending any reports.
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultNotSupported,
            first_callback_result_);
}

TEST_F(HidHapticGamepadBaseTest, StartDelayTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  // This test establishes the behavior for the start_delay parameter when
  // PlayEffect is called without preempting an existing effect.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      0.0,
      base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                     base::Unretained(this)));

  task_runner_->RunUntilIdle();

  // With zero start_delay, exactly two reports are issued, to start and stop
  // the effect.
  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_,
                                   stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);

  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      1.0,
      base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                     base::Unretained(this)));

  task_runner_->RunUntilIdle();

  EXPECT_EQ(2, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
}

TEST_F(HidHapticGamepadBaseTest, ZeroStartDelayPreemptionTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      0.0,
      base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                     base::Unretained(this)));

  // Start a second effect with zero start_delay. This should cause the first
  // effect to be preempted before it calls SetVibration.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      0.0,
      base::BindOnce(&HidHapticGamepadBaseTest::SecondCallback,
                     base::Unretained(this)));

  // Execute the pending tasks, but stop before executing any newly queued
  // tasks.
  task_runner_->RunPendingTasks();

  // The first effect should have already returned with a "preempted" result
  // without issuing a report. The second effect is not yet started.
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);

  task_runner_->RunPendingTasks();

  // The second effect should have been started.
  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  task_runner_->RunUntilIdle();

  // Now the second effect should have returned with a "complete" result.
  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_,
                                   stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
}

TEST_F(HidHapticGamepadBaseTest, NonZeroStartDelayPreemptionTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      0.0,
      base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                     base::Unretained(this)));

  // Start a second effect with non-zero start_delay. This should cause the
  // first effect to be preempted before it calls SetVibration.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      1.0,
      base::BindOnce(&HidHapticGamepadBaseTest::SecondCallback,
                     base::Unretained(this)));

  // Execute the pending tasks, but stop before executing any newly queued
  // tasks.
  task_runner_->RunPendingTasks();

  // The first effect should have already returned with a "preempted" result.
  // Because the second effect has a non-zero start_delay and is preempting
  // another effect, it will call SetZeroVibration to ensure no vibration
  // occurs during its start_delay period.
  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);

  task_runner_->RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_,
                                   stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
}

TEST_F(HidHapticGamepadBaseTest, ResetVibrationPreemptionTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt it with a reset.
  PostPlayEffect(
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble, 1.0,
      0.0,
      base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                     base::Unretained(this)));

  // Reset vibration. This should cause the effect to be preempted before it
  // calls SetVibration.
  PostResetVibration(base::BindOnce(&HidHapticGamepadBaseTest::SecondCallback,
                                    base::Unretained(this)));

  task_runner_->RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
}

}  // namespace

}  // namespace device
