// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace {

using ::testing::DoAll;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::_;

class ScriptExecutorTest : public testing::Test, public ScriptExecutorDelegate {
 public:
  void SetUp() override {
    executor_ = std::make_unique<ScriptExecutor>("script path", this);

    // In this test, "tell" actions always succeed and "click" actions always
    // fail.
    ON_CALL(mock_web_controller_, OnClickElement(_, _))
        .WillByDefault(RunOnceCallback<1>(false));
  }

 protected:
  ScriptExecutorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {}

  Service* GetService() override { return &mock_service_; }

  UiController* GetUiController() override { return &mock_ui_controller_; }

  WebController* GetWebController() override { return &mock_web_controller_; }

  ClientMemory* GetClientMemory() override { return &memory_; }

  std::string Serialize(const google::protobuf::MessageLite& message) {
    std::string output;
    message.SerializeToString(&output);
    return output;
  }

  // scoped_task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  Script script_;
  ClientMemory memory_;
  StrictMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;
  NiceMock<MockUiController> mock_ui_controller_;
  std::unique_ptr<ScriptExecutor> executor_;
  StrictMock<base::MockCallback<ScriptExecutor::RunScriptCallback>>
      executor_callback_;
};

TEST_F(ScriptExecutorTest, GetActionsFails) {
  EXPECT_CALL(mock_service_, OnGetActions(_, _))
      .WillOnce(RunOnceCallback<1>(false, ""));
  EXPECT_CALL(executor_callback_, Run(false));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, RunOneActionReportFailureAndStop) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");

  EXPECT_CALL(mock_service_, OnGetActions(_, _))
      .WillOnce(RunOnceCallback<1>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions_capture),
                      RunOnceCallback<2>(true, "")));
  EXPECT_CALL(executor_callback_, Run(true));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunMultipleActions) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.set_server_payload("payload1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq("script path"), _))
      .WillOnce(RunOnceCallback<1>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_server_payload("payload2");
  next_actions_response.add_actions()->mutable_tell()->set_message("3");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&processed_actions1_capture),
                RunOnceCallback<2>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions2_capture),
                      RunOnceCallback<2>(true, "")));
  EXPECT_CALL(executor_callback_, Run(true));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(1u, processed_actions2_capture.size());
}

TEST_F(ScriptExecutorTest, InterruptActionListOnError) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.set_server_payload("payload");
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "will pass");
  initial_actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "never run");

  EXPECT_CALL(mock_service_, OnGetActions(_, _))
      .WillOnce(RunOnceCallback<1>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_server_payload("payload2");
  next_actions_response.add_actions()->mutable_tell()->set_message(
      "will run after error");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&processed_actions1_capture),
                RunOnceCallback<2>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions2_capture),
                      RunOnceCallback<2>(true, "")));
  EXPECT_CALL(executor_callback_, Run(true));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions1_capture[1].status());

  ASSERT_EQ(1u, processed_actions2_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
  // make sure "never run" wasn't the one that was run.
  EXPECT_EQ("will run after error",
            processed_actions2_capture[0].action().tell().message());
}

TEST_F(ScriptExecutorTest, RunDelayedAction) {
  ActionsResponseProto actions_response;
  actions_response.set_server_payload("payload");
  ActionProto* action = actions_response.add_actions();
  action->mutable_tell()->set_message("delayed");
  action->set_action_delay_ms(1000);

  EXPECT_CALL(mock_service_, OnGetActions(_, _))
      .WillOnce(RunOnceCallback<1>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(DoAll(SaveArg<1>(&processed_actions_capture),
                      RunOnceCallback<2>(true, "")));

  // executor_callback_.Run() not expected to be run just yet, as the action is
  // delayed.
  executor_->Run(executor_callback_.Get());
  EXPECT_TRUE(scoped_task_environment_.MainThreadHasPendingTask());

  // Moving forward in time triggers action execution.
  EXPECT_CALL(executor_callback_, Run(true));
  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(scoped_task_environment_.MainThreadHasPendingTask());
}

}  // namespace
}  // namespace autofill_assistant
