// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

class PromptActionTest : public testing::Test {
 public:
  PromptActionTest()
      : task_env_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementCheck(_, _))
        .WillByDefault(RunOnceCallback<1>(false));
    ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
        .WillByDefault(RunOnceCallback<1>(false, ""));

    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    ON_CALL(mock_action_delegate_, Prompt(_))
        .WillByDefault(Invoke([this](std::unique_ptr<std::vector<Chip>> chips) {
          chips_ = std::move(chips);
        }));
    prompt_proto_ = proto_.mutable_prompt();
  }

 protected:
  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment task_env_;

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  PromptProto* prompt_proto_;
  std::unique_ptr<std::vector<Chip>> chips_;
};

TEST_F(PromptActionTest, ChoicesMissing) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  PromptAction action(proto_);
  action.ProcessAction(&mock_action_delegate_, callback_.Get());
}

TEST_F(PromptActionTest, SelectButtons) {
  auto* ok_proto = prompt_proto_->add_choices();
  auto* chip = ok_proto->mutable_chip();
  chip->set_text("Ok");
  chip->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  auto* cancel_proto = prompt_proto_->add_choices();
  cancel_proto->set_name("Cancel");
  cancel_proto->set_chip_type(NORMAL_ACTION);
  cancel_proto->set_server_payload("cancel");

  PromptAction action(proto_);
  action.ProcessAction(&mock_action_delegate_, callback_.Get());

  ASSERT_THAT(chips_, Pointee(SizeIs(2)));

  EXPECT_EQ("Ok", (*chips_)[0].text);
  EXPECT_EQ(HIGHLIGHTED_ACTION, (*chips_)[0].type);

  EXPECT_EQ("Cancel", (*chips_)[1].text);
  EXPECT_EQ(NORMAL_ACTION, (*chips_)[1].type);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::prompt_choice,
                   Property(&PromptProto::Choice::server_payload, "ok"))))));
  DCHECK((*chips_)[0].callback);
  std::move((*chips_)[0].callback).Run();
}

TEST_F(PromptActionTest, ShowOnlyIfElementExists) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->set_name("Ok");
  ok_proto->set_chip_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->add_show_only_if_element_exists()->add_selectors("element");

  PromptAction action(proto_);
  action.ProcessAction(&mock_action_delegate_, callback_.Get());

  ASSERT_THAT(chips_, Pointee(IsEmpty()));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(true));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(chips_, Pointee(SizeIs(1)));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(false));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(chips_, Pointee(IsEmpty()));
}

TEST_F(PromptActionTest, DisabledUnlessElementExists) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->set_name("Ok");
  ok_proto->set_chip_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->set_allow_disabling(true);
  ok_proto->add_show_only_if_element_exists()->add_selectors("element");

  PromptAction action(proto_);
  action.ProcessAction(&mock_action_delegate_, callback_.Get());

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(true));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(chips_, Pointee(SizeIs(1)));
  EXPECT_FALSE((*chips_)[0].disabled);

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(false));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(chips_, Pointee(SizeIs(1)));
  EXPECT_TRUE((*chips_)[0].disabled);
}

TEST_F(PromptActionTest, AutoSelect) {
  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  choice_proto->mutable_auto_select_if_element_exists()->add_selectors(
      "element");

  PromptAction action(proto_);
  action.ProcessAction(&mock_action_delegate_, callback_.Get());
  EXPECT_THAT(chips_, Pointee(SizeIs(0)));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(true));

  EXPECT_CALL(mock_action_delegate_, CancelPrompt());
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::prompt_choice,
                                 Property(&PromptProto::Choice::server_payload,
                                          "auto-select"))))));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(PromptActionTest, AutoSelectWithButton) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->set_name("Ok");
  ok_proto->set_chip_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  choice_proto->mutable_auto_select_if_element_exists()->add_selectors(
      "element");

  PromptAction action(proto_);
  action.ProcessAction(&mock_action_delegate_, callback_.Get());

  ASSERT_THAT(chips_, Pointee(SizeIs(1)));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(true));
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::prompt_choice,
                                 Property(&PromptProto::Choice::server_payload,
                                          "auto-select"))))));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(PromptActionTest, Terminate) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->set_name("Ok");
  ok_proto->set_chip_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  {
    PromptAction action(proto_);
    action.ProcessAction(&mock_action_delegate_, callback_.Get());
  }

  // Chips pointing to a deleted action do nothing.
  ASSERT_THAT(chips_, Pointee(SizeIs(1)));
  std::move((*chips_)[0].callback).Run();
}

}  // namespace
}  // namespace autofill_assistant
