// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion_test_helpers.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kInvalidUsername[] = "no-username";
const char kPasswordName[] = "password";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";

using autofill::Suggestion;
using autofill::SuggestionVectorIdsAre;
using autofill::SuggestionVectorValuesAre;
using testing::_;

namespace autofill {
class AutofillPopupDelegate;
}

namespace password_manager {

namespace {

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD2(FillSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD2(PreviewSuggestion,
               void(const base::string16&, const base::string16&));
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerDriver* mock_driver() { return &driver_; }

 private:
  MockPasswordManagerDriver driver_;
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MOCK_METHOD4(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<Suggestion>& suggestions,
                    base::WeakPtr<autofill::AutofillPopupDelegate> delegate));
  MOCK_METHOD0(HideAutofillPopup, void());
};

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(base::ASCIIToUTF16(kAliceUsername)),
        test_password_(base::ASCIIToUTF16(kAlicePassword)),
        fill_data_id_(0) {}

  void SetUp() override {
    // Add a preferred login and an additional login to the FillData.
    autofill::FormFieldData username_field;
    username_field.name = base::ASCIIToUTF16(kUsernameName);
    username_field.value = test_username_;
    fill_data_.username_field = username_field;

    autofill::FormFieldData password_field;
    password_field.name = base::ASCIIToUTF16(kPasswordName);
    password_field.value = test_password_;
    fill_data_.password_field = password_field;
  }

  void InitializePasswordAutofillManager(
      TestPasswordManagerClient* client,
      autofill::AutofillClient* autofill_client) {
    password_autofill_manager_.reset(
        new PasswordAutofillManager(client->mock_driver(), autofill_client));
    password_autofill_manager_->OnAddPasswordFormMapping(fill_data_id_,
                                                         fill_data_);
  }

 protected:
  int fill_data_id() { return fill_data_id_; }

  scoped_ptr<PasswordAutofillManager> password_autofill_manager_;

  base::string16 test_username_;
  base::string16 test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;
  const int fill_data_id_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::MessageLoop message_loop_;
};

TEST_F(PasswordAutofillManagerTest, FillSuggestion) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  InitializePasswordAutofillManager(client.get(), nullptr);

  EXPECT_CALL(*client->mock_driver(),
              FillSuggestion(test_username_, test_password_));
  EXPECT_TRUE(password_autofill_manager_->FillSuggestionForTest(
      fill_data_id(), test_username_));
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());

  EXPECT_CALL(*client->mock_driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      fill_data_id(), base::ASCIIToUTF16(kInvalidUsername)));

  const int invalid_fill_data_id = fill_data_id() + 1;

  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      invalid_fill_data_id, test_username_));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      fill_data_id(), test_username_));
}

TEST_F(PasswordAutofillManagerTest, PreviewSuggestion) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  InitializePasswordAutofillManager(client.get(), nullptr);

  EXPECT_CALL(*client->mock_driver(),
              PreviewSuggestion(test_username_, test_password_));
  EXPECT_TRUE(password_autofill_manager_->PreviewSuggestionForTest(
      fill_data_id(), test_username_));
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());

  EXPECT_CALL(*client->mock_driver(), PreviewSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      fill_data_id(), base::ASCIIToUTF16(kInvalidUsername)));

  const int invalid_fill_data_id = fill_data_id() + 1;

  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      invalid_fill_data_id, test_username_));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      fill_data_id(), test_username_));
}

// Test that the popup is marked as visible after recieving password
// suggestions.
TEST_F(PasswordAutofillManagerTest, ExternalDelegatePasswordSuggestions) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  scoped_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.preferred_realm = "http://foo.com/";
  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(*client->mock_driver(),
              FillSuggestion(test_username_, test_password_));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  _,
                  _,
                  SuggestionVectorIdsAre(testing::ElementsAre(
                      autofill::POPUP_ITEM_ID_PASSWORD_ENTRY)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::string16(), false,
      element_bounds);

  // Accepting a suggestion should trigger a call to hide the popup.
  EXPECT_CALL(*autofill_client, HideAutofillPopup());
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
}

// Test that OnShowPasswordSuggestions correctly matches the given FormFieldData
// to the known PasswordFormFillData, and extracts the right suggestions.
TEST_F(PasswordAutofillManagerTest, ExtractSuggestions) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  scoped_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("John Different"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // First, simulate displaying suggestions matching an empty prefix.
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                      test_username_, additional_username, other_username)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::string16(), false,
      element_bounds);

  // Now simulate displaying suggestions matching "John".
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                      additional_username,
                      other_username)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("John"), false,
      element_bounds);

  // Finally, simulate displaying all suggestions, without any prefix matching.
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                      test_username_, additional_username, other_username)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("xyz"), true,
      element_bounds);
}

TEST_F(PasswordAutofillManagerTest, FillSuggestionPasswordField) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  scoped_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("John Different"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // Simulate displaying suggestions matching a username and specifying that the
  // field is a password field.
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE);
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                      title,
                      test_username_)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
}

}  // namespace password_manager
