// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/autofill_action.h"

#include "base/bind.h"
#include "base/guid.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_client_memory.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrNe;

class MockPersonalDataManager : public autofill::PersonalDataManager {
 public:
  MockPersonalDataManager() : PersonalDataManager("en-US") {}
  ~MockPersonalDataManager() override{};

  // PersonalDataManager:
  std::string SaveImportedProfile(
      const autofill::AutofillProfile& profile) override {
    std::vector<autofill::AutofillProfile> profiles;
    std::string merged_guid =
        MergeProfile(profile, &profiles_, "en-US", &profiles);
    if (merged_guid == profile.guid())
      profiles_.push_back(std::make_unique<autofill::AutofillProfile>(profile));
    return merged_guid;
  }

  autofill::AutofillProfile* GetProfileByGUID(
      const std::string& guid) override {
    autofill::AutofillProfile* result = nullptr;
    for (const auto& profile : profiles_) {
      if (profile->guid() != guid)
        continue;
      result = profile.get();
      break;
    }

    return result;
  }

 private:
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles_;

  DISALLOW_COPY_AND_ASSIGN(MockPersonalDataManager);
};

// A callback that expects to be called immediately.
//
// This relies on mocked methods calling their callbacks immediately (which is
// the case in this test).
class DirectCallback {
 public:
  DirectCallback() : was_run_(false), result_(nullptr) {}

  // Returns a base::OnceCallback. The current instance must exist until
  // GetResultOrDie is called.
  base::OnceCallback<void(std::unique_ptr<ProcessedActionProto>)> Get() {
    return base::BindOnce(&DirectCallback::Run, base::Unretained(this));
  }

  ProcessedActionProto* GetResultOrDie() {
    CHECK(was_run_);
    return result_.get();
  }

 private:
  void Run(std::unique_ptr<ProcessedActionProto> result) {
    was_run_ = true;
    result_ = std::move(result);
  }

  bool was_run_;
  std::unique_ptr<ProcessedActionProto> result_;
};

class AutofillActionTest : public testing::Test {
 public:
  void SetUp() override {
    autofill::AutofillProfile profile(base::GenerateGUID(),
                                      autofill::test::kEmptyOrigin);
    autofill::test::SetProfileInfo(&profile, kFirstName, "", kLastName, kEmail,
                                   "", "", "", "", "", "", "", "");
    autofill_profile_guid_ = profile.guid();
    personal_data_manager_ = std::make_unique<MockPersonalDataManager>();
    personal_data_manager_->SaveImportedProfile(profile);

    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&mock_client_memory_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(personal_data_manager_.get()));
  }

 protected:
  const char* const kAddressName = "billing";
  const char* const kFakeSelector = "#selector";
  const char* const kSelectionPrompt = "prompt";
  const char* const kFirstName = "Foo";
  const char* const kLastName = "Bar";
  const char* const kEmail = "foobar@gmail.com";

  ActionProto CreateUseAddressAction() {
    ActionProto action;
    UseAddressProto* use_address = action.mutable_use_address();
    use_address->set_name(kAddressName);
    use_address->mutable_form_field_element()->add_selectors(kFakeSelector);
    return action;
  }

  ActionProto CreateUseCardAction() {
    ActionProto action;
    action.mutable_use_card()->mutable_form_field_element()->add_selectors(
        kFakeSelector);
    return action;
  }

  bool ProcessAction(const ActionProto& action_proto) {
    AutofillAction action(action_proto);
    // We can use DirectCallback given that methods in ActionDelegate are mocked
    // and return directly.
    DirectCallback callback;
    action.ProcessAction(&mock_action_delegate_, callback.Get());
    return callback.GetResultOrDie()->status() ==
           ProcessedActionStatus::ACTION_APPLIED;
  }

  MockActionDelegate mock_action_delegate_;
  MockClientMemory mock_client_memory_;
  std::string autofill_profile_guid_;
  std::unique_ptr<autofill::PersonalDataManager> personal_data_manager_;
};

TEST_F(AutofillActionTest, FillManually) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);

  // No selection was made previously.
  EXPECT_CALL(mock_client_memory_, selected_address(kAddressName))
      .WillOnce(Return(base::nullopt));

  // Expect prompt.
  EXPECT_CALL(mock_action_delegate_, ShowStatusMessage(kSelectionPrompt));

  // Return empty address guid (manual filling).
  EXPECT_CALL(mock_action_delegate_, OnChooseAddress(_))
      .WillOnce(RunOnceCallback<0>(""));

  // We save the selection in memory.
  EXPECT_CALL(mock_client_memory_, set_selected_address(kAddressName, ""));

  EXPECT_FALSE(ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, ValidationSucceeds) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  int required_fields_count = 3;
  for (int i = 0; i < required_fields_count; i++) {
    auto* required_field =
        action_proto.mutable_use_address()->add_required_fields();
    required_field->set_address_field(
        UseAddressProto::RequiredField::FIRST_NAME);
    required_field->mutable_element()->add_selectors(kFakeSelector);
  }

  // Return a fake selected address.
  EXPECT_CALL(mock_client_memory_, selected_address(kAddressName))
      .WillOnce(Return(autofill_profile_guid_));

  // Autofill succeeds.
  EXPECT_CALL(
      mock_action_delegate_,
      OnFillAddressForm(autofill_profile_guid_, ElementsAre(kFakeSelector), _))
      .WillOnce(RunOnceCallback<2>(true));

  // Validation succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnGetFieldValue(ElementsAre(kFakeSelector), _))
      .Times(required_fields_count)
      .WillRepeatedly(RunOnceCallback<1>("not empty"));

  EXPECT_TRUE(ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, FallbackFails) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  std::vector<UseAddressProto::RequiredField::AddressField> address_fields = {
      UseAddressProto::RequiredField::FIRST_NAME,
      UseAddressProto::RequiredField::LAST_NAME,
      UseAddressProto::RequiredField::EMAIL};

  for (size_t i = 0; i < address_fields.size(); i++) {
    auto* required_field =
        action_proto.mutable_use_address()->add_required_fields();
    required_field->set_address_field(address_fields[i]);
    required_field->mutable_element()->add_selectors(kFakeSelector);
  }

  // Return a fake selected address.
  EXPECT_CALL(mock_client_memory_, selected_address(kAddressName))
      .WillOnce(Return(autofill_profile_guid_));

  // Autofill succeeds.
  EXPECT_CALL(
      mock_action_delegate_,
      OnFillAddressForm(autofill_profile_guid_, ElementsAre(kFakeSelector), _))
      .WillOnce(RunOnceCallback<2>(true));

  // Validation fails when getting FIRST_NAME.
  EXPECT_CALL(mock_action_delegate_,
              OnGetFieldValue(ElementsAre(kFakeSelector), _))
      .Times(address_fields.size())
      .WillOnce(RunOnceCallback<1>(""))
      .WillRepeatedly(RunOnceCallback<1>("not empty"));

  // Fallback fails.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(ElementsAre(kFakeSelector), kFirstName, _))
      .WillOnce(RunOnceCallback<2>(false));

  EXPECT_FALSE(ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, FallbackSucceeds) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  std::vector<UseAddressProto::RequiredField::AddressField> address_fields = {
      UseAddressProto::RequiredField::FIRST_NAME,
      UseAddressProto::RequiredField::LAST_NAME,
      UseAddressProto::RequiredField::EMAIL};

  for (size_t i = 0; i < address_fields.size(); i++) {
    auto* required_field =
        action_proto.mutable_use_address()->add_required_fields();
    required_field->set_address_field(address_fields[i]);
    required_field->mutable_element()->add_selectors(kFakeSelector);
  }

  // Return a fake selected address.
  EXPECT_CALL(mock_client_memory_, selected_address(kAddressName))
      .WillOnce(Return(autofill_profile_guid_));

  // Autofill succeeds.
  EXPECT_CALL(
      mock_action_delegate_,
      OnFillAddressForm(autofill_profile_guid_, ElementsAre(kFakeSelector), _))
      .WillOnce(RunOnceCallback<2>(true));

  // Validation fails when getting FIRST_NAME.
  EXPECT_CALL(mock_action_delegate_,
              OnGetFieldValue(ElementsAre(kFakeSelector), _))
      .Times(address_fields.size())
      .WillOnce(RunOnceCallback<1>(""))
      .WillRepeatedly(RunOnceCallback<1>("not empty"));

  // Fallback succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(ElementsAre(kFakeSelector), kFirstName, _))
      .WillOnce(RunOnceCallback<2>(true));

  // Second validation succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnGetFieldValue(ElementsAre(kFakeSelector), _))
      .Times(address_fields.size())
      .WillRepeatedly(RunOnceCallback<1>("not empty"));

  EXPECT_TRUE(ProcessAction(action_proto));
}

}  // namespace
}  // namespace autofill_assistant
