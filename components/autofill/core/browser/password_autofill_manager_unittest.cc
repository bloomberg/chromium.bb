// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/password_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kInvalidUsername[] = "no-username";
const char kPasswordName[] = "password";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";

namespace {

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  MockAutofillDriver() {}
  MOCK_METHOD1(RendererShouldAcceptPasswordAutofillSuggestion,
               void(const base::string16&));
};

}  // namespace

namespace autofill {

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest() :
      password_autofill_manager_(&autofill_driver_) {}

  virtual void SetUp() OVERRIDE {
    // Add a preferred login and an additional login to the FillData.
    base::string16 username1 = base::ASCIIToUTF16(kAliceUsername);
    base::string16 password1 = base::ASCIIToUTF16(kAlicePassword);

    username_field_.name = base::ASCIIToUTF16(kUsernameName);
    username_field_.value = username1;
    fill_data_.basic_data.fields.push_back(username_field_);

    FormFieldData password_field;
    password_field.name = base::ASCIIToUTF16(kPasswordName);
    password_field.value = password1;
    fill_data_.basic_data.fields.push_back(password_field);

    password_autofill_manager_.AddPasswordFormMapping(username_field_,
                                                      fill_data_);
  }

  MockAutofillDriver* autofill_driver() {
    return &autofill_driver_;
  }

  PasswordAutofillManager* password_autofill_manager() {
    return &password_autofill_manager_;
  }

  const FormFieldData& username_field() { return username_field_; }

 private:
  PasswordFormFillData fill_data_;
  FormFieldData username_field_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::MessageLoop message_loop_;
  MockAutofillDriver autofill_driver_;
  PasswordAutofillManager password_autofill_manager_;
};

TEST_F(PasswordAutofillManagerTest, DidAcceptAutofillSuggestion) {
  EXPECT_CALL(*autofill_driver(),
      RendererShouldAcceptPasswordAutofillSuggestion(
          base::ASCIIToUTF16(kAliceUsername)));
  EXPECT_TRUE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      username_field(), base::ASCIIToUTF16(kAliceUsername)));

  EXPECT_CALL(*autofill_driver(),
      RendererShouldAcceptPasswordAutofillSuggestion(
          base::ASCIIToUTF16(kInvalidUsername))).Times(0);
  EXPECT_FALSE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      username_field(), base::ASCIIToUTF16(kInvalidUsername)));

  FormFieldData invalid_username_field;
  invalid_username_field.name = base::ASCIIToUTF16(kInvalidUsername);

  EXPECT_FALSE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      invalid_username_field, base::ASCIIToUTF16(kAliceUsername)));

  password_autofill_manager()->Reset();
  EXPECT_FALSE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      username_field(), base::ASCIIToUTF16(kAliceUsername)));
}

}  // namespace autofill
