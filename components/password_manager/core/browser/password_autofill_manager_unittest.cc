// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_delegate.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kInvalidUsername[] = "no-username";
const char kPasswordName[] = "password";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";

using testing::_;

namespace autofill {
class AutofillPopupDelegate;
}

namespace {

// TODO(dubroy): Implement a TestPasswordManagerDriver that can be shared by
// all the tests that need it (crbug.com/352566).
class MockPasswordManagerDriver
    : public password_manager::PasswordManagerDriver {
 public:
  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD0(GetPasswordGenerationManager,
               password_manager::PasswordGenerationManager*());
  MOCK_METHOD0(GetPasswordManager, password_manager::PasswordManager*());
  MOCK_METHOD0(GetAutofillManager, autofill::AutofillManager*());
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(autofill::PasswordForm*));
  MOCK_METHOD1(AccountCreationFormsFound,
               void(const std::vector<autofill::FormData>&));
  MOCK_METHOD2(AcceptPasswordAutofillSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD0(GetPasswordAutofillManager,
               password_manager::PasswordAutofillManager*());
};

class TestPasswordManagerClient
    : public password_manager::PasswordManagerClient {
 public:
  virtual void PromptUserToSavePassword(
      password_manager::PasswordFormManager* form_to_save) OVERRIDE {}
  virtual password_manager::PasswordStore* GetPasswordStore() OVERRIDE {
    return NULL;
  }
  virtual PrefService* GetPrefs() OVERRIDE { return NULL; }
  virtual password_manager::PasswordManagerDriver* GetDriver() OVERRIDE {
    return &driver_;
  }
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) OVERRIDE {}
  virtual bool IsPasswordSyncEnabled() OVERRIDE { return false; }

  MockPasswordManagerDriver* mock_driver() { return &driver_; }

 private:
  MockPasswordManagerDriver driver_;
};

class MockAutofillManagerDelegate
    : public autofill::TestAutofillManagerDelegate {
 public:
  MOCK_METHOD7(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<base::string16>& values,
                    const std::vector<base::string16>& labels,
                    const std::vector<base::string16>& icons,
                    const std::vector<int>& identifiers,
                    base::WeakPtr<autofill::AutofillPopupDelegate> delegate));
  MOCK_METHOD0(HideAutofillPopup, void());
};

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(base::ASCIIToUTF16(kAliceUsername)),
        test_password_(base::ASCIIToUTF16(kAlicePassword)) {}

  virtual void SetUp() OVERRIDE {
    // Add a preferred login and an additional login to the FillData.
    username_field_.name = base::ASCIIToUTF16(kUsernameName);
    username_field_.value = test_username_;
    fill_data_.basic_data.fields.push_back(username_field_);

    autofill::FormFieldData password_field;
    password_field.name = base::ASCIIToUTF16(kPasswordName);
    password_field.value = test_password_;
    fill_data_.basic_data.fields.push_back(password_field);
  }

  void InitializePasswordAutofillManager(
      password_manager::PasswordManagerClient* client,
      autofill::AutofillManagerDelegate* autofill_manager_delegate) {
    password_autofill_manager_.reset(
        new password_manager::PasswordAutofillManager(
            client, autofill_manager_delegate));
    password_autofill_manager_->OnAddPasswordFormMapping(username_field_,
                                                         fill_data_);
  }

 protected:
  scoped_ptr<password_manager::PasswordAutofillManager>
      password_autofill_manager_;

  autofill::FormFieldData username_field_;
  base::string16 test_username_;
  base::string16 test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::MessageLoop message_loop_;
};

TEST_F(PasswordAutofillManagerTest, AcceptSuggestion) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  InitializePasswordAutofillManager(client.get(), NULL);

  EXPECT_CALL(*client->mock_driver(),
              AcceptPasswordAutofillSuggestion(test_username_, test_password_));
  EXPECT_TRUE(password_autofill_manager_->AcceptSuggestionForTest(
      username_field_, test_username_));
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());

  EXPECT_CALL(*client->mock_driver(),
              AcceptPasswordAutofillSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->AcceptSuggestionForTest(
      username_field_, base::ASCIIToUTF16(kInvalidUsername)));

  autofill::FormFieldData invalid_username_field;
  invalid_username_field.name = base::ASCIIToUTF16(kInvalidUsername);

  EXPECT_FALSE(password_autofill_manager_->AcceptSuggestionForTest(
      invalid_username_field, test_username_));

  password_autofill_manager_->Reset();
  EXPECT_FALSE(password_autofill_manager_->AcceptSuggestionForTest(
      username_field_, test_username_));
}

// Test that the popup is marked as visible after recieving password
// suggestions.
TEST_F(PasswordAutofillManagerTest, ExternalDelegatePasswordSuggestions) {
  scoped_ptr<TestPasswordManagerClient> client(new TestPasswordManagerClient);
  scoped_ptr<MockAutofillManagerDelegate> delegate(
      new MockAutofillManagerDelegate);
  InitializePasswordAutofillManager(client.get(), delegate.get());

  gfx::RectF element_bounds;
  std::vector<base::string16> suggestions;
  suggestions.push_back(test_username_);
  std::vector<base::string16> realms;
  realms.push_back(base::ASCIIToUTF16("http://foo.com/"));

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(
      *delegate,
      ShowAutofillPopup(_, _, _, _, _,
                        testing::ElementsAre(
                            autofill::POPUP_ITEM_ID_PASSWORD_ENTRY),
                        _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      username_field_, element_bounds, suggestions, realms);

  // Accepting a suggestion should trigger a call to hide the popup.
  EXPECT_CALL(*delegate, HideAutofillPopup());
  password_autofill_manager_->DidAcceptSuggestion(
      suggestions[0], autofill::POPUP_ITEM_ID_PASSWORD_ENTRY);
}
