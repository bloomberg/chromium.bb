// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <string>

#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::CONFIRMATION_PASSWORD;
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::NEW_PASSWORD;
using autofill::PASSWORD;
using autofill::PasswordForm;
using autofill::ServerFieldTypeSet;
using autofill::SubmissionIndicatorEvent;
using base::ASCIIToUTF16;
using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::Return;
using testing::SaveArg;

namespace password_manager {
namespace {

constexpr int kNumberOfPasswordAttributes =
    static_cast<int>(autofill::PasswordAttribute::kPasswordAttributesCount);

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr, &fake_observer) {}

  MOCK_METHOD6(StartUploadRequest,
               bool(const FormStructure&,
                    bool,
                    const autofill::ServerFieldTypeSet&,
                    const std::string&,
                    bool,
                    PrefService*));

 private:
  class StubObserver : public AutofillDownloadManager::Observer {
    void OnLoadedServerPredictions(
        std::string response,
        const std::vector<std::string>& form_signatures) override {}
  };

  StubObserver fake_observer;
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD0(GetAutofillDownloadManager,
               autofill::AutofillDownloadManager*());
};

}  // namespace

class VotesUploaderTest : public testing::Test {
 public:
  VotesUploaderTest() {
    EXPECT_CALL(client_, GetAutofillDownloadManager())
        .WillRepeatedly(Return(&mock_autofill_download_manager_));

    ON_CALL(mock_autofill_download_manager_,
            StartUploadRequest(_, _, _, _, _, _))
        .WillByDefault(Return(true));

    // Create |fields| in |form_to_upload_| and |submitted_form_|. Only |name|
    // field in FormFieldData is important. Set them to the unique values based
    // on index.
    const size_t kNumberOfFields = 20;
    for (size_t i = 0; i < kNumberOfFields; ++i) {
      FormFieldData field;
      field.name = GetFieldNameByIndex(i);
      form_to_upload_.form_data.fields.push_back(field);
      submitted_form_.form_data.fields.push_back(field);
    }
  }

 protected:
  base::string16 GetFieldNameByIndex(size_t index) {
    return ASCIIToUTF16("field") + base::UintToString16(index);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockAutofillDownloadManager mock_autofill_download_manager_;

  MockPasswordManagerClient client_;

  PasswordForm form_to_upload_;
  PasswordForm submitted_form_;

  std::string login_form_signature_ = "123";
};

TEST_F(VotesUploaderTest, UploadPasswordVoteUpdate) {
  VotesUploader votes_uploader(&client_, true);
  base::string16 new_password_element = GetFieldNameByIndex(3);
  base::string16 confirmation_element = GetFieldNameByIndex(11);
  form_to_upload_.new_password_element = new_password_element;
  submitted_form_.new_password_element = new_password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {NEW_PASSWORD,
                                             CONFIRMATION_PASSWORD};
  FieldTypeMap expected_types = {{new_password_element, NEW_PASSWORD},
                                 {confirmation_element, CONFIRMATION_PASSWORD}};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(form_to_upload_),
                UploadedAutofillTypesAre(expected_types),
                SubmissionEventIsSameAs(expected_submission_event)),
          false, expected_field_types, login_form_signature_, true, nullptr));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, NEW_PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, UploadPasswordVoteSave) {
  VotesUploader votes_uploader(&client_, false);
  base::string16 password_element = GetFieldNameByIndex(5);
  base::string16 confirmation_element = GetFieldNameByIndex(12);
  form_to_upload_.password_element = password_element;
  submitted_form_.password_element = password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  SubmissionEventIsSameAs(expected_submission_event), false,
                  expected_field_types, login_form_signature_, true,
                  /* pref_service= */ nullptr));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote) {
  VotesUploader votes_uploader(&client_, true);
  // Checks that randomization distorts information about present and missed
  // character classess, but a true value is still restorable with aggregation
  // of many distorted reports.
  const char* kPasswordSnippets[] = {"abc", "XYZ", "123", "*-_"};
  for (int test_case = 0; test_case < 10; ++test_case) {
    bool has_password_attribute[kNumberOfPasswordAttributes];
    base::string16 password_value;
    for (int i = 0; i < kNumberOfPasswordAttributes; ++i) {
      has_password_attribute[i] = base::RandGenerator(2);
      if (has_password_attribute[i])
        password_value += ASCIIToUTF16(kPasswordSnippets[i]);
    }
    if (password_value.empty())
      continue;

    autofill::FormData form;
    autofill::FormStructure form_structure(form);
    int reported_false[kNumberOfPasswordAttributes] = {0, 0, 0, 0};
    int reported_true[kNumberOfPasswordAttributes] = {0, 0, 0, 0};

    int reported_actual_length = 0;
    int reported_wrong_length = 0;

    for (int i = 0; i < 1000; ++i) {
      votes_uploader.GeneratePasswordAttributesVote(password_value,
                                                    &form_structure);
      base::Optional<std::pair<autofill::PasswordAttribute, bool>> vote =
          form_structure.get_password_attributes_vote_for_testing();
      int attribute_index = static_cast<int>(vote->first);
      if (vote->second)
        reported_true[attribute_index]++;
      else
        reported_false[attribute_index]++;
      size_t reported_length =
          form_structure.get_password_length_vote_for_testing();
      if (reported_length == password_value.size()) {
        reported_actual_length++;
      } else {
        reported_wrong_length++;
        EXPECT_LT(0u, reported_length);
        EXPECT_LT(reported_length, password_value.size());
      }
    }
    for (int i = 0; i < kNumberOfPasswordAttributes; i++) {
      EXPECT_LT(0, reported_false[i]);
      EXPECT_LT(0, reported_true[i]);

      // If the actual value is |true|, then it should report more |true|s than
      // |false|s.
      if (has_password_attribute[i]) {
        EXPECT_LT(reported_false[i], reported_true[i])
            << "Wrong distribution for attribute " << i
            << ". password_value = " << password_value;
      } else {
        EXPECT_GT(reported_false[i], reported_true[i])
            << "Wrong distribution for attribute " << i
            << ". password_value = " << password_value;
      }
    }
    EXPECT_LT(0, reported_actual_length);
    EXPECT_LT(0, reported_wrong_length);
    EXPECT_LT(reported_actual_length, reported_wrong_length);
  }
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_OneCharacterPassword) {
  // |VotesUploader::GeneratePasswordAttributesVote| shouldn't crash if a
  // password has only one character.
  autofill::FormData form;
  autofill::FormStructure form_structure(form);
  VotesUploader votes_uploader(&client_, true);
  votes_uploader.GeneratePasswordAttributesVote(ASCIIToUTF16("1"),
                                                &form_structure);
  base::Optional<std::pair<autofill::PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote_for_testing();
  EXPECT_TRUE(vote.has_value());
  size_t reported_length =
      form_structure.get_password_length_vote_for_testing();
  EXPECT_EQ(1u, reported_length);
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_AllAsciiCharacters) {
  autofill::FormData form;
  autofill::FormStructure form_structure(form);
  VotesUploader votes_uploader(&client_, true);
  votes_uploader.GeneratePasswordAttributesVote(
      base::UTF8ToUTF16("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqr"
                        "stuvwxyz!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"),
      &form_structure);
  base::Optional<std::pair<autofill::PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote_for_testing();
  EXPECT_TRUE(vote.has_value());
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_NonAsciiPassword) {
  // Checks that password attributes vote is not generated if the password has
  // non-ascii characters.
  for (const auto* password :
       {"пароль1", "パスワード", "münchen", "סיסמה-A", "Σ-12345",
        "գաղտնաբառըTTT", "Slaptažodis", "密碼", "كلمهالسر", "mậtkhẩu!",
        "ລະຫັດຜ່ານ-l", "စကားဝှက်ကို3", "პაროლი", "पारण शब्द"}) {
    autofill::FormData form;
    autofill::FormStructure form_structure(form);
    VotesUploader votes_uploader(&client_, true);
    votes_uploader.GeneratePasswordAttributesVote(base::UTF8ToUTF16(password),
                                                  &form_structure);
    base::Optional<std::pair<autofill::PasswordAttribute, bool>> vote =
        form_structure.get_password_attributes_vote_for_testing();

    EXPECT_FALSE(vote.has_value()) << password;
  }
}

}  // namespace password_manager
