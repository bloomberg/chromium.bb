// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/virtual_time_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "headless/public/internal/headless_devtools_client_impl.h"
#include "headless/public/util/testing/mock_devtools_agent_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

using testing::ElementsAre;
using testing::Mock;
using testing::Return;
using testing::_;

class VirtualTimeControllerTest : public ::testing::Test {
 protected:
  VirtualTimeControllerTest() {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    client_.SetTaskRunnerForTests(task_runner_);
    mock_host_ = base::MakeRefCounted<MockDevToolsAgentHost>();

    EXPECT_CALL(*mock_host_, IsAttached()).WillOnce(Return(false));
    EXPECT_CALL(*mock_host_, AttachClient(&client_));
    client_.AttachToHost(mock_host_.get());
    controller_ = std::make_unique<VirtualTimeController>(&client_, 0);
  }

  ~VirtualTimeControllerTest() override = default;

  void GrantVirtualTimeBudget(int budget_ms) {
    ASSERT_FALSE(set_up_complete_);
    ASSERT_FALSE(budget_expired_);

    controller_->GrantVirtualTimeBudget(
        emulation::VirtualTimePolicy::ADVANCE,
        base::TimeDelta::FromMilliseconds(budget_ms),
        base::Bind(
            [](bool* set_up_complete) {
              EXPECT_FALSE(*set_up_complete);
              *set_up_complete = true;
            },
            base::Unretained(&set_up_complete_)),
        base::Bind(
            [](bool* budget_expired) {
              EXPECT_FALSE(*budget_expired);
              *budget_expired = true;
            },
            base::Unretained(&budget_expired_)));

    EXPECT_FALSE(set_up_complete_);
    EXPECT_FALSE(budget_expired_);
  }

  void SendVirtualTimeBudgetExpiredEvent() {
    client_.DispatchProtocolMessage(
        mock_host_.get(),
        "{\"method\":\"Emulation.virtualTimeBudgetExpired\",\"params\":{}}");
    // Events are dispatched asynchronously.
    task_runner_->RunPendingTasks();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<MockDevToolsAgentHost> mock_host_;
  HeadlessDevToolsClientImpl client_;
  std::unique_ptr<VirtualTimeController> controller_;

  bool set_up_complete_ = false;
  bool budget_expired_ = false;
};

TEST_F(VirtualTimeControllerTest, AdvancesTimeWithoutTasks) {
  controller_ = std::make_unique<VirtualTimeController>(&client_, 1000);

  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":0,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":5000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":1000,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  GrantVirtualTimeBudget(5000);
}

TEST_F(VirtualTimeControllerTest, MaxVirtualTimeTaskStarvationCount) {
  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":0,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":5000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  GrantVirtualTimeBudget(5000);

  client_.DispatchProtocolMessage(
      mock_host_.get(), "{\"id\":0,\"result\":{\"virtualTimeBase\":1.0}}");

  EXPECT_TRUE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  SendVirtualTimeBudgetExpiredEvent();

  EXPECT_TRUE(budget_expired_);
}

namespace {
class MockTask : public VirtualTimeController::RepeatingTask {
 public:
  MOCK_METHOD2(IntervalElapsed,
               void(base::TimeDelta virtual_time_offset,
                    const base::Callback<void()>& continue_callback));
  MOCK_METHOD3(BudgetRequested,
               void(base::TimeDelta virtual_time_offset,
                    base::TimeDelta requested_budget,
                    const base::Callback<void()>& continue_callback));
  MOCK_METHOD1(BudgetExpired, void(base::TimeDelta virtual_time_offset));
};

ACTION_TEMPLATE(RunClosure,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  ::std::tr1::get<k>(args).Run();
}

ACTION_P(RunClosure, closure) {
  closure.Run();
};
}  // namespace

TEST_F(VirtualTimeControllerTest, InterleavesTasksWithVirtualTime) {
  MockTask task;
  controller_->ScheduleRepeatingTask(&task,
                                     base::TimeDelta::FromMilliseconds(1000));

  EXPECT_CALL(task, BudgetRequested(base::TimeDelta::FromMilliseconds(0),
                                    base::TimeDelta::FromMilliseconds(3000), _))
      .WillOnce(RunClosure<2>());
  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":0,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":1000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  GrantVirtualTimeBudget(3000);

  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  client_.DispatchProtocolMessage(
      mock_host_.get(), "{\"id\":0,\"result\":{\"virtualTimeBase\":1.0}}");

  EXPECT_TRUE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  // We check that set_up_complete_callback is only run once, so reset it here.
  set_up_complete_ = false;

  for (int i = 1; i < 3; i++) {
    EXPECT_CALL(task,
                IntervalElapsed(base::TimeDelta::FromMilliseconds(1000 * i), _))
        .WillOnce(RunClosure<1>());
    EXPECT_CALL(
        *mock_host_,
        DispatchProtocolMessage(
            &client_,
            base::StringPrintf(
                "{\"id\":%d,\"method\":\"Emulation.setVirtualTimePolicy\","
                "\"params\":{\"budget\":1000.0,"
                "\"maxVirtualTimeTaskStarvationCount\":0,"
                "\"policy\":\"advance\"}}",
                i * 2)))
        .WillOnce(Return(true));

    SendVirtualTimeBudgetExpiredEvent();

    EXPECT_FALSE(set_up_complete_);
    EXPECT_FALSE(budget_expired_);

    client_.DispatchProtocolMessage(
        mock_host_.get(),
        base::StringPrintf("{\"id\":%d,\"result\":{\"virtualTimeBase\":1.0}}",
                           i * 2));

    EXPECT_FALSE(set_up_complete_);
    EXPECT_FALSE(budget_expired_);
  }

  EXPECT_CALL(task, IntervalElapsed(base::TimeDelta::FromMilliseconds(3000), _))
      .WillOnce(RunClosure<1>());
  EXPECT_CALL(task, BudgetExpired(base::TimeDelta::FromMilliseconds(3000)));
  SendVirtualTimeBudgetExpiredEvent();

  EXPECT_FALSE(set_up_complete_);
  EXPECT_TRUE(budget_expired_);
}

TEST_F(VirtualTimeControllerTest, CanceledTask) {
  MockTask task;
  controller_->ScheduleRepeatingTask(&task,
                                     base::TimeDelta::FromMilliseconds(1000));

  EXPECT_CALL(task, BudgetRequested(base::TimeDelta::FromMilliseconds(0),
                                    base::TimeDelta::FromMilliseconds(5000), _))
      .WillOnce(RunClosure<2>());
  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":0,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":1000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  GrantVirtualTimeBudget(5000);

  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  client_.DispatchProtocolMessage(
      mock_host_.get(), "{\"id\":0,\"result\":{\"virtualTimeBase\":1.0}}");

  EXPECT_TRUE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  // We check that set_up_complete_callback is only run once, so reset it here.
  set_up_complete_ = false;

  EXPECT_CALL(task, IntervalElapsed(base::TimeDelta::FromMilliseconds(1000), _))
      .WillOnce(RunClosure<1>());
  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":2,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":1000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  SendVirtualTimeBudgetExpiredEvent();

  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  client_.DispatchProtocolMessage(
      mock_host_.get(),
      base::StringPrintf("{\"id\":2,\"result\":{\"virtualTimeBase\":1.0}}"));

  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  controller_->CancelRepeatingTask(&task);

  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":4,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":3000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  SendVirtualTimeBudgetExpiredEvent();

  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  client_.DispatchProtocolMessage(
      mock_host_.get(),
      base::StringPrintf("{\"id\":4,\"result\":{\"virtualTimeBase\":1.0}}"));

  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  SendVirtualTimeBudgetExpiredEvent();

  EXPECT_FALSE(set_up_complete_);
  EXPECT_TRUE(budget_expired_);
}

TEST_F(VirtualTimeControllerTest, MultipleTasks) {
  MockTask task1;
  MockTask task2;
  controller_->ScheduleRepeatingTask(&task1,
                                     base::TimeDelta::FromMilliseconds(1000));
  controller_->ScheduleRepeatingTask(&task2,
                                     base::TimeDelta::FromMilliseconds(1000));

  EXPECT_CALL(task1,
              BudgetRequested(base::TimeDelta::FromMilliseconds(0),
                              base::TimeDelta::FromMilliseconds(2000), _))
      .WillOnce(RunClosure<2>());
  EXPECT_CALL(task2,
              BudgetRequested(base::TimeDelta::FromMilliseconds(0),
                              base::TimeDelta::FromMilliseconds(2000), _))
      .WillOnce(RunClosure<2>());
  // We should only get one call to Emulation.setVirtualTimePolicy despite
  // having two tasks.
  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":0,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":1000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  GrantVirtualTimeBudget(2000);
  EXPECT_FALSE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);

  client_.DispatchProtocolMessage(
      mock_host_.get(),
      base::StringPrintf("{\"id\":0,\"result\":{\"virtualTimeBase\":1.0}}"));

  EXPECT_TRUE(set_up_complete_);
  EXPECT_FALSE(budget_expired_);
}

class VirtualTimeTask : public VirtualTimeController::RepeatingTask {
 public:
  using Task = base::Callback<void(base::TimeDelta virtual_time)>;

  VirtualTimeTask(VirtualTimeController* controller,
                  Task budget_requested_task,
                  Task interval_elapsed_task,
                  Task budget_expired_task)
      : controller_(controller),
        budget_requested_task_(budget_requested_task),
        interval_elapsed_task_(interval_elapsed_task),
        budget_expired_task_(budget_expired_task) {}

  void IntervalElapsed(
      base::TimeDelta virtual_time,
      const base::Callback<void()>& continue_callback) override {
    interval_elapsed_task_.Run(virtual_time);
    continue_callback.Run();
  }

  void BudgetRequested(
      base::TimeDelta virtual_time,
      base::TimeDelta requested_budget_ms,
      const base::Callback<void()>& continue_callback) override {
    budget_requested_task_.Run(virtual_time);
    continue_callback.Run();
  }

  void BudgetExpired(base::TimeDelta virtual_time) override {
    budget_expired_task_.Run(virtual_time);
  };

  VirtualTimeController* controller_;  // NOT OWNED
  Task budget_requested_task_;
  Task interval_elapsed_task_;
  Task budget_expired_task_;
};

TEST_F(VirtualTimeControllerTest, ReentrantTask) {
#if defined(__clang__)
  std::vector<std::string> log;
  VirtualTimeTask task_b(
      controller_.get(),
      base::Bind(
          [](std::vector<std::string>* log, base::TimeDelta virtual_time) {
            log->push_back(base::StringPrintf(
                "B: budget requested @ %d",
                static_cast<int>(virtual_time.InMilliseconds())));
          },
          &log),
      base::Bind(
          [](std::vector<std::string>* log, VirtualTimeController* controller,
             VirtualTimeTask* task_b, base::TimeDelta virtual_time) {
            log->push_back(base::StringPrintf(
                "B: interval elapsed @ %d",
                static_cast<int>(virtual_time.InMilliseconds())));
            controller->CancelRepeatingTask(task_b);
          },
          &log, controller_.get(), &task_b),
      base::Bind(
          [](std::vector<std::string>* log, base::TimeDelta virtual_time) {
            log->push_back(base::StringPrintf(
                "B: budget expired @ %d",
                static_cast<int>(virtual_time.InMilliseconds())));
          },
          &log));

  VirtualTimeTask task_a(
      controller_.get(),
      base::Bind(
          [](std::vector<std::string>* log, base::TimeDelta virtual_time) {
            log->push_back(base::StringPrintf(
                "A: budget requested @ %d",
                static_cast<int>(virtual_time.InMilliseconds())));
          },
          &log),
      base::Bind(
          [](std::vector<std::string>* log, VirtualTimeController* controller,
             VirtualTimeTask* task_a, VirtualTimeTask* task_b,
             base::TimeDelta virtual_time) {
            log->push_back(base::StringPrintf(
                "A: interval elapsed @ %d",
                static_cast<int>(virtual_time.InMilliseconds())));
            controller->CancelRepeatingTask(task_a);
            controller->ScheduleRepeatingTask(
                task_b, base::TimeDelta::FromMilliseconds(1500));
          },
          &log, controller_.get(), &task_a, &task_b),
      base::Bind(
          [](std::vector<std::string>* log, base::TimeDelta virtual_time) {
            log->push_back(base::StringPrintf(
                "A: budget expired @ %d",
                static_cast<int>(virtual_time.InMilliseconds())));
          },
          &log));

  controller_->ScheduleRepeatingTask(&task_a,
                                     base::TimeDelta::FromMilliseconds(1000));

  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":0,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":1000.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));

  GrantVirtualTimeBudget(6000);
  Mock::VerifyAndClearExpectations(&mock_host_);

  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":2,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":1500.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));
  SendVirtualTimeBudgetExpiredEvent();
  Mock::VerifyAndClearExpectations(&mock_host_);

  EXPECT_CALL(*mock_host_,
              DispatchProtocolMessage(
                  &client_,
                  "{\"id\":4,\"method\":\"Emulation.setVirtualTimePolicy\","
                  "\"params\":{\"budget\":3500.0,"
                  "\"maxVirtualTimeTaskStarvationCount\":0,"
                  "\"policy\":\"advance\"}}"))
      .WillOnce(Return(true));
  SendVirtualTimeBudgetExpiredEvent();

  EXPECT_THAT(
      log, ElementsAre("A: budget requested @ 0", "A: interval elapsed @ 1000",
                       "B: interval elapsed @ 2500"));
#endif
}

}  // namespace headless
