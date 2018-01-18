// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestAutofillManager::TestAutofillManager(AutofillDriver* driver,
                                         AutofillClient* client,
                                         TestPersonalDataManager* personal_data)
    : AutofillManager(driver, client, personal_data),
      personal_data_(personal_data),
      context_getter_(driver->GetURLRequestContext()) {
  set_payments_client(new payments::PaymentsClient(
      context_getter_, client->GetPrefs(), client->GetIdentityProvider(),
      /*unmask_delegate=*/this,
      /*save_delegate=*/nullptr));
}

TestAutofillManager::TestAutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    TestPersonalDataManager* personal_data,
    std::unique_ptr<CreditCardSaveManager> credit_card_save_manager,
    payments::TestPaymentsClient* payments_client)
    : AutofillManager(driver, client, personal_data),
      personal_data_(personal_data),
      test_form_data_importer_(
          new TestFormDataImporter(client,
                                   payments_client,
                                   std::move(credit_card_save_manager),
                                   personal_data,
                                   "en-US")) {
  set_payments_client(payments_client);
  set_form_data_importer(test_form_data_importer_);
}

TestAutofillManager::~TestAutofillManager() {}

bool TestAutofillManager::IsAutofillEnabled() const {
  return autofill_enabled_;
}

bool TestAutofillManager::IsCreditCardAutofillEnabled() {
  return credit_card_enabled_;
}

void TestAutofillManager::UploadFormData(const FormStructure& submitted_form,
                                         bool observed_submission) {
  submitted_form_signature_ = submitted_form.FormSignatureAsStr();

  if (call_parent_upload_form_data_)
    AutofillManager::UploadFormData(submitted_form, observed_submission);
}

void TestAutofillManager::UploadFormDataAsyncCallback(
    const FormStructure* submitted_form,
    const base::TimeTicks& load_time,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time,
    bool observed_submission) {
  run_loop_->Quit();

  if (expected_observed_submission_ != base::nullopt)
    EXPECT_EQ(expected_observed_submission_, observed_submission);

  // If we have expected field types set, make sure they match.
  if (!expected_submitted_field_types_.empty()) {
    ASSERT_EQ(expected_submitted_field_types_.size(),
              submitted_form->field_count());
    for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "Field %d with value %s", static_cast<int>(i),
          base::UTF16ToUTF8(submitted_form->field(i)->value).c_str()));
      const ServerFieldTypeSet& possible_types =
          submitted_form->field(i)->possible_types();
      EXPECT_EQ(expected_submitted_field_types_[i].size(),
                possible_types.size());
      for (ServerFieldTypeSet::const_iterator it =
               expected_submitted_field_types_[i].begin();
           it != expected_submitted_field_types_[i].end(); ++it) {
        EXPECT_TRUE(possible_types.count(*it))
            << "Expected type: " << AutofillType(*it).ToString();
      }
    }
  }

  AutofillManager::UploadFormDataAsyncCallback(
      submitted_form, load_time, interaction_time, submission_time,
      observed_submission);
}

void TestAutofillManager::ResetRunLoop() {
  run_loop_.reset(new base::RunLoop());
}

void TestAutofillManager::RunRunLoop() {
  run_loop_->Run();
}

void TestAutofillManager::WaitForAsyncUploadProcess() {
  run_loop_->Run();
}

int TestAutofillManager::GetPackedCreditCardID(int credit_card_id) {
  std::string credit_card_guid =
      base::StringPrintf("00000000-0000-0000-0000-%012d", credit_card_id);

  return MakeFrontendID(credit_card_guid, std::string());
}

void TestAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  FormData empty_form = form;
  for (size_t i = 0; i < empty_form.fields.size(); ++i) {
    empty_form.fields[i].value = base::string16();
  }

  std::unique_ptr<TestFormStructure> form_structure =
      std::make_unique<TestFormStructure>(empty_form);
  form_structure->SetFieldTypes(heuristic_types, server_types);
  AddSeenFormStructure(std::move(form_structure));

  form_interactions_ukm_logger()->OnFormsParsed(
      form.main_frame_origin.GetURL());
}

void TestAutofillManager::AddSeenFormStructure(
    std::unique_ptr<FormStructure> form_structure) {
  form_structure->set_form_parsed_timestamp(base::TimeTicks::Now());
  form_structures()->push_back(std::move(form_structure));
}

void TestAutofillManager::SubmitForm(const FormData& form,
                                     const TimeTicks& timestamp) {
  ResetRunLoop();
  if (!OnFormSubmitted(form, false, SubmissionSource::FORM_SUBMISSION,
                       timestamp))
    return;

  RunRunLoop();
}

void TestAutofillManager::SubmitForm(const FormData& form) {
  SubmitForm(form, TimeTicks::Now());
}

void TestAutofillManager::ClearFormStructures() {
  form_structures()->clear();
}

const std::string TestAutofillManager::GetSubmittedFormSignature() {
  return submitted_form_signature_;
}

void TestAutofillManager::SetAutofillEnabled(bool autofill_enabled) {
  autofill_enabled_ = autofill_enabled;
}

void TestAutofillManager::SetCreditCardEnabled(bool credit_card_enabled) {
  credit_card_enabled_ = credit_card_enabled;
  if (!credit_card_enabled_)
    // Credit card data is refreshed when this pref is changed.
    personal_data_->ClearCreditCards();
}

void TestAutofillManager::SetExpectedSubmittedFieldTypes(
    const std::vector<ServerFieldTypeSet>& expected_types) {
  expected_submitted_field_types_ = expected_types;
}

void TestAutofillManager::SetExpectedObservedSubmission(bool expected) {
  expected_observed_submission_ = expected;
}

void TestAutofillManager::SetCallParentUploadFormData(bool value) {
  call_parent_upload_form_data_ = value;
}

}  // namespace autofill
