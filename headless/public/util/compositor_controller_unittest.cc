// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/compositor_controller.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "headless/public/internal/headless_devtools_client_impl.h"
#include "headless/public/util/testing/mock_devtools_agent_host.h"
#include "headless/public/util/virtual_time_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

namespace {
static constexpr base::TimeDelta kAnimationFrameInterval =
    base::TimeDelta::FromMilliseconds(16);
static constexpr base::TimeDelta kWaitForCompositorReadyFrameDelay =
    base::TimeDelta::FromMilliseconds(20);
} // namespace

using testing::Return;
using testing::_;

class TestVirtualTimeController : public VirtualTimeController {
 public:
  TestVirtualTimeController(HeadlessDevToolsClient* devtools_client)
      : VirtualTimeController(devtools_client) {}
  ~TestVirtualTimeController() override = default;

  MOCK_METHOD4(GrantVirtualTimeBudget,
               void(emulation::VirtualTimePolicy policy,
                    base::TimeDelta budget,
                    const base::Callback<void()>& set_up_complete_callback,
                    const base::Callback<void()>& budget_expired_callback));
  MOCK_METHOD2(ScheduleRepeatingTask,
               void(RepeatingTask* task, base::TimeDelta interval));
  MOCK_METHOD1(CancelRepeatingTask, void(RepeatingTask* task));

  MOCK_CONST_METHOD0(GetVirtualTimeBase, base::Time());
  MOCK_CONST_METHOD0(GetCurrentVirtualTimeOffset, base::TimeDelta());
};

class CompositorControllerTest : public ::testing::Test {
 protected:
  CompositorControllerTest(bool update_display_for_animations = true) {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    client_.SetTaskRunnerForTests(task_runner_);
    mock_host_ = base::MakeRefCounted<MockDevToolsAgentHost>();

    EXPECT_CALL(*mock_host_, IsAttached()).WillOnce(Return(false));
    EXPECT_CALL(*mock_host_, AttachClient(&client_));
    client_.AttachToHost(mock_host_.get());
    virtual_time_controller_ =
        std::make_unique<TestVirtualTimeController>(&client_);
    EXPECT_CALL(*virtual_time_controller_,
                ScheduleRepeatingTask(_, kAnimationFrameInterval))
        .WillOnce(testing::SaveArg<0>(&task_));
    ExpectHeadlessExperimentalEnable();
    controller_ = std::make_unique<CompositorController>(
        task_runner_, &client_, virtual_time_controller_.get(),
        kAnimationFrameInterval, kWaitForCompositorReadyFrameDelay,
        update_display_for_animations);
    EXPECT_NE(nullptr, task_);
  }

  ~CompositorControllerTest() override {
    EXPECT_CALL(*virtual_time_controller_, CancelRepeatingTask(_));
  }

  void ExpectHeadlessExperimentalEnable() {
    last_command_id_ += 2;
    EXPECT_CALL(*mock_host_,
                DispatchProtocolMessage(
                    &client_,
                    base::StringPrintf(
                        "{\"id\":%d,\"method\":\"HeadlessExperimental.enable\","
                        "\"params\":{}}",
                        last_command_id_)))
        .WillOnce(Return(true));
  }

  void ExpectVirtualTime(double base, double offset) {
    auto base_time = base::Time::FromJsTime(base);
    auto offset_delta = base::TimeDelta::FromMilliseconds(offset);
    EXPECT_CALL(*virtual_time_controller_, GetVirtualTimeBase())
        .WillOnce(Return(base_time));
    EXPECT_CALL(*virtual_time_controller_, GetCurrentVirtualTimeOffset())
        .WillOnce(Return(offset_delta));

    // Next BeginFrame's time should be the virtual time provided it has
    // progressed.
    base::Time virtual_time = base_time + offset_delta;
    if (virtual_time > next_begin_frame_time_)
      next_begin_frame_time_ = virtual_time;
  }

  void ExpectBeginFrame(bool no_display_updates = false,
                        std::unique_ptr<headless_experimental::ScreenshotParams>
                            screenshot_params = nullptr) {
    last_command_id_ += 2;
    base::DictionaryValue params;
    auto builder =
        std::move(headless_experimental::BeginFrameParams::Builder()
                      .SetFrameTime(next_begin_frame_time_.ToJsTime())
                      .SetInterval(kAnimationFrameInterval.InMillisecondsF())
                      .SetNoDisplayUpdates(no_display_updates));
    if (screenshot_params)
      builder.SetScreenshot(std::move(screenshot_params));
    // Subsequent BeginFrames should have a later timestamp.
    next_begin_frame_time_ += base::TimeDelta::FromMicroseconds(1);

    std::string params_json;
    auto params_value = builder.Build()->Serialize();
    base::JSONWriter::Write(*params_value, &params_json);

    EXPECT_CALL(
        *mock_host_,
        DispatchProtocolMessage(
            &client_,
            base::StringPrintf(
                "{\"id\":%d,\"method\":\"HeadlessExperimental.beginFrame\","
                "\"params\":%s}",
                last_command_id_, params_json.c_str())))
        .WillOnce(Return(true));
  }

  void SendBeginFrameReply(bool has_damage,
                           bool main_frame_content_updated,
                           const std::string& screenshot_data) {
    auto result = headless_experimental::BeginFrameResult::Builder()
                      .SetHasDamage(has_damage)
                      .SetMainFrameContentUpdated(main_frame_content_updated)
                      .Build();
    if (screenshot_data.length())
      result->SetScreenshotData(screenshot_data);
    std::string result_json;
    auto result_value = result->Serialize();
    base::JSONWriter::Write(*result_value, &result_json);

    client_.DispatchProtocolMessage(
        mock_host_.get(),
        base::StringPrintf("{\"id\":%d,\"result\":%s}", last_command_id_,
                           result_json.c_str()));
  }

  void SendNeedsBeginFramesEvent(bool needs_begin_frames) {
    client_.DispatchProtocolMessage(
        mock_host_.get(),
        base::StringPrintf("{\"method\":\"HeadlessExperimental."
                           "needsBeginFramesChanged\",\"params\":{"
                           "\"needsBeginFrames\":%s}}",
                           needs_begin_frames ? "true" : "false"));
    // Events are dispatched asynchronously.
    task_runner_->RunPendingTasks();
  }

  void SendMainFrameReadyForScreenshotsEvent() {
    client_.DispatchProtocolMessage(mock_host_.get(),
                                    "{\"method\":\"HeadlessExperimental."
                                    "mainFrameReadyForScreenshots\",\"params\":"
                                    "{}}");
    // Events are dispatched asynchronously.
    task_runner_->RunPendingTasks();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<MockDevToolsAgentHost> mock_host_;
  HeadlessDevToolsClientImpl client_;
  std::unique_ptr<TestVirtualTimeController> virtual_time_controller_;
  std::unique_ptr<CompositorController> controller_;
  int last_command_id_ = -2;
  TestVirtualTimeController::RepeatingTask* task_ = nullptr;
  base::Time next_begin_frame_time_ =
      base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(1);
};

TEST_F(CompositorControllerTest, WaitForCompositorReady) {
  // Shouldn't send any commands yet as no needsBeginFrames event was sent yet.
  bool ready = false;
  controller_->WaitForCompositorReady(
      base::BindRepeating([](bool* ready) { *ready = true; }, &ready));
  EXPECT_FALSE(ready);

  // Sends BeginFrames with delay while they are needed.
  SendNeedsBeginFramesEvent(true);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  SendBeginFrameReply(true, false, std::string());

  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_EQ(kWaitForCompositorReadyFrameDelay,
            task_runner_->NextPendingTaskDelay());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  EXPECT_FALSE(task_runner_->HasPendingTask());

  SendBeginFrameReply(false, false, std::string());

  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_EQ(kWaitForCompositorReadyFrameDelay,
            task_runner_->NextPendingTaskDelay());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // No new BeginFrames are scheduled when BeginFrames are not needed.
  SendNeedsBeginFramesEvent(false);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Restarts sending BeginFrames when they are needed again.
  SendNeedsBeginFramesEvent(true);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // Stops sending BeginFrames when main frame becomes ready.
  SendMainFrameReadyForScreenshotsEvent();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  EXPECT_FALSE(ready);
  SendBeginFrameReply(true, true, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(ready);
}

TEST_F(CompositorControllerTest, CaptureScreenshot) {
  SendMainFrameReadyForScreenshotsEvent();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  bool done = false;
  controller_->CaptureScreenshot(
      headless_experimental::ScreenshotParamsFormat::PNG, 100,
      base::BindRepeating(
          [](bool* done, const std::string& screenshot_data) {
            *done = true;
            EXPECT_EQ("test", screenshot_data);
          },
          &done));

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame(
      false, headless_experimental::ScreenshotParams::Builder()
                 .SetFormat(headless_experimental::ScreenshotParamsFormat::PNG)
                 .SetQuality(100)
                 .Build());
  task_runner_->RunPendingTasks();

  std::string base64;
  base::Base64Encode("test", &base64);
  SendBeginFrameReply(true, true, base64);
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(done);
}

TEST_F(CompositorControllerTest, WaitForMainFrameContentUpdate) {
  bool updated = false;
  controller_->WaitForMainFrameContentUpdate(
      base::BindRepeating([](bool* updated) { *updated = true; }, &updated));
  EXPECT_FALSE(updated);

  // Sends BeginFrames while they are needed.
  SendNeedsBeginFramesEvent(true);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  SendBeginFrameReply(true, false, std::string());

  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_EQ(base::TimeDelta(), task_runner_->NextPendingTaskDelay());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  EXPECT_FALSE(task_runner_->HasPendingTask());

  SendBeginFrameReply(false, false, std::string());

  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_EQ(base::TimeDelta(), task_runner_->NextPendingTaskDelay());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // No new BeginFrames are scheduled when BeginFrames are not needed.
  SendNeedsBeginFramesEvent(false);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Restarts sending BeginFrames when they are needed again.
  SendNeedsBeginFramesEvent(true);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // Stops sending BeginFrames when an main frame update is included.
  SendMainFrameReadyForScreenshotsEvent();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  EXPECT_FALSE(updated);
  SendBeginFrameReply(true, true, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(updated);
}

TEST_F(CompositorControllerTest, SendsAnimationFrames) {
  bool can_continue = false;
  auto continue_callback = base::BindRepeating(
      [](bool* can_continue) { *can_continue = true; }, &can_continue);

  // Task doesn't block virtual time request.
  task_->BudgetRequested(base::TimeDelta(),
                         base::TimeDelta::FromMilliseconds(100),
                         continue_callback);
  EXPECT_TRUE(can_continue);
  can_continue = false;

  // Doesn't send BeginFrames before virtual time started.
  SendNeedsBeginFramesEvent(true);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Sends a BeginFrame after interval elapsed.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(1000, kAnimationFrameInterval.InMillisecondsF());
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // Lets virtual time continue after BeginFrame was completed.
  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(can_continue);
  can_continue = false;

  // Sends another BeginFrame after next interval elapsed.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(1000, kAnimationFrameInterval.InMillisecondsF() * 2);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // Lets virtual time continue after BeginFrame was completed.
  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(can_continue);
  can_continue = false;
}

TEST_F(CompositorControllerTest, SkipsAnimationFrameForScreenshots) {
  bool can_continue = false;
  auto continue_callback = base::BindRepeating(
      [](bool* can_continue) { *can_continue = true; }, &can_continue);

  SendMainFrameReadyForScreenshotsEvent();
  EXPECT_FALSE(task_runner_->HasPendingTask());
  SendNeedsBeginFramesEvent(true);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Doesn't send a BeginFrame after interval elapsed if a screenshot is taken
  // instead.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  controller_->CaptureScreenshot(
      headless_experimental::ScreenshotParamsFormat::PNG, 100,
      base::BindRepeating([](const std::string&) {}));

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame(
      false, headless_experimental::ScreenshotParams::Builder()
                 .SetFormat(headless_experimental::ScreenshotParamsFormat::PNG)
                 .SetQuality(100)
                 .Build());
  task_runner_->RunPendingTasks();

  EXPECT_TRUE(can_continue);
  can_continue = false;
}

TEST_F(CompositorControllerTest,
       PostponesAnimationFrameWhenBudgetExpired) {
  bool can_continue = false;
  auto continue_callback = base::BindRepeating(
      [](bool* can_continue) { *can_continue = true; }, &can_continue);

  SendNeedsBeginFramesEvent(true);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Doesn't send a BeginFrame after interval elapsed if the budget also
  // expired.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  task_->BudgetExpired(kAnimationFrameInterval);
  EXPECT_TRUE(can_continue);
  can_continue = false;
  // Flush cancelled task.
  task_runner_->RunPendingTasks();

  // Sends a BeginFrame when more virtual time budget is requested.
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_->BudgetRequested(kAnimationFrameInterval,
                         base::TimeDelta::FromMilliseconds(100),
                         continue_callback);
  EXPECT_FALSE(can_continue);

  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(can_continue);
  can_continue = false;
}

TEST_F(CompositorControllerTest,
       SkipsAnimationFrameWhenBudgetExpiredAndScreenshotWasTaken) {
  bool can_continue = false;
  auto continue_callback = base::BindRepeating(
      [](bool* can_continue) { *can_continue = true; }, &can_continue);

  SendMainFrameReadyForScreenshotsEvent();
  EXPECT_FALSE(task_runner_->HasPendingTask());
  SendNeedsBeginFramesEvent(true);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Doesn't send a BeginFrame after interval elapsed if the budget also
  // expired.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  task_->BudgetExpired(kAnimationFrameInterval);
  EXPECT_TRUE(can_continue);
  can_continue = false;
  // Flush cancelled task.
  task_runner_->RunPendingTasks();

  controller_->CaptureScreenshot(
      headless_experimental::ScreenshotParamsFormat::PNG, 100,
      base::BindRepeating([](const std::string&) {}));

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame(
      false, headless_experimental::ScreenshotParams::Builder()
                 .SetFormat(headless_experimental::ScreenshotParamsFormat::PNG)
                 .SetQuality(100)
                 .Build());
  task_runner_->RunPendingTasks();

  // Sends a BeginFrame when more virtual time budget is requested.
  task_->BudgetRequested(kAnimationFrameInterval,
                         base::TimeDelta::FromMilliseconds(100),
                         continue_callback);
  EXPECT_TRUE(can_continue);
  can_continue = false;
}

TEST_F(CompositorControllerTest, WaitUntilIdle) {
  bool idle = false;
  auto idle_callback =
      base::BindRepeating([](bool* idle) { *idle = true; }, &idle);

  SendNeedsBeginFramesEvent(true);
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // WaitUntilIdle executes callback immediately if no BeginFrame is active.
  controller_->WaitUntilIdle(idle_callback);
  EXPECT_TRUE(idle);
  idle = false;

  // Send a BeginFrame.
  task_->IntervalElapsed(kAnimationFrameInterval, base::BindRepeating([]() {}));

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(0, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  // WaitUntilIdle only executes callback after BeginFrame was completed.
  controller_->WaitUntilIdle(idle_callback);
  EXPECT_FALSE(idle);

  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(idle);
  idle = false;
}

class CompositorControllerNoDisplayUpdateTest
    : public CompositorControllerTest {
 protected:
  CompositorControllerNoDisplayUpdateTest() : CompositorControllerTest(false) {}
};

TEST_F(CompositorControllerNoDisplayUpdateTest,
       SkipsDisplayUpdateOnlyForAnimationFrames) {
  bool can_continue = false;
  auto continue_callback = base::BindRepeating(
      [](bool* can_continue) { *can_continue = true; }, &can_continue);

  // Wait for update BeginFrames update display.
  SendNeedsBeginFramesEvent(true);
  controller_->WaitForMainFrameContentUpdate(base::BindRepeating([]() {}));
  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(1000, 0);
  ExpectBeginFrame();
  task_runner_->RunPendingTasks();

  SendMainFrameReadyForScreenshotsEvent();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  SendBeginFrameReply(true, true, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Sends an animation BeginFrame without display update after interval
  // elapsed.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(1000, kAnimationFrameInterval.InMillisecondsF());
  ExpectBeginFrame(true);
  task_runner_->RunPendingTasks();

  // Lets virtual time continue after BeginFrame was completed.
  SendBeginFrameReply(false, false, std::string());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_TRUE(can_continue);
  can_continue = false;

  // Screenshots update display.
  task_->IntervalElapsed(kAnimationFrameInterval, continue_callback);
  EXPECT_FALSE(can_continue);

  controller_->CaptureScreenshot(
      headless_experimental::ScreenshotParamsFormat::PNG, 100,
      base::BindRepeating([](const std::string&) {}));

  EXPECT_TRUE(task_runner_->HasPendingTask());
  ExpectVirtualTime(1000, kAnimationFrameInterval.InMillisecondsF() * 2);
  ExpectBeginFrame(
      false, headless_experimental::ScreenshotParams::Builder()
                 .SetFormat(headless_experimental::ScreenshotParamsFormat::PNG)
                 .SetQuality(100)
                 .Build());
  task_runner_->RunPendingTasks();

  EXPECT_TRUE(can_continue);
  can_continue = false;
}

}  // namespace headless
