// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/password_autofill_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The name of the username/password element in the form.
const char* const kUsernameName = "username";
const char* const kInvalidUsername = "no-username";
const char* const kPasswordName = "password";

const char* const kAliceUsername = "alice";
const char* const kAlicePassword = "password";

const char* const kValue = "password";

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest() : password_autofill_manager_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Add a preferred login and an additional login to the FillData.
    string16 username1 = ASCIIToUTF16(kAliceUsername);
    string16 password1 = ASCIIToUTF16(kAlicePassword);

    username_field_.name = ASCIIToUTF16(kUsernameName);
    username_field_.value = username1;
    fill_data_.basic_data.fields.push_back(username_field_);

    FormFieldData password_field;
    password_field.name = ASCIIToUTF16(kPasswordName);
    password_field.value = password1;
    fill_data_.basic_data.fields.push_back(password_field);

    password_autofill_manager_.AddPasswordFormMapping(username_field_,
                                                      fill_data_);
  }

  PasswordAutofillManager* password_autofill_manager() {
    return &password_autofill_manager_;
  }

  const FormFieldData& username_field() { return username_field_; }

 private:
  PasswordFormFillData fill_data_;
  FormFieldData username_field_;

  PasswordAutofillManager password_autofill_manager_;
};

TEST_F(PasswordAutofillManagerTest, DidAcceptAutofillSuggestion) {
  EXPECT_TRUE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      username_field(), ASCIIToUTF16(kAliceUsername)));
  EXPECT_FALSE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      username_field(), ASCIIToUTF16(kInvalidUsername)));

  FormFieldData invalid_username_field;
  invalid_username_field.name = ASCIIToUTF16(kInvalidUsername);

  EXPECT_FALSE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      invalid_username_field, ASCIIToUTF16(kAliceUsername)));

  password_autofill_manager()->Reset();
  EXPECT_FALSE(password_autofill_manager()->DidAcceptAutofillSuggestion(
      username_field(), ASCIIToUTF16(kAliceUsername)));
}
