// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validating_textfield.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

class ValidatingTextfieldTest : public testing::Test {
 public:
  ValidatingTextfieldTest() {}
  ~ValidatingTextfieldTest() override {}

 protected:
  class ValidationDelegate : public ValidatingTextfield::Delegate {
   public:
    ValidationDelegate() {}
    ~ValidationDelegate() override {}

    // ValidatingTextfield::Delegate
    bool ValidateTextfield(views::Textfield* textfield) override {
      // We really don't like textfields with more than 5 characters in them.
      return textfield->text().size() <= 5u;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ValidationDelegate);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(ValidatingTextfieldTest);
};

TEST_F(ValidatingTextfieldTest, Validation) {
  std::unique_ptr<ValidationDelegate> delegate(new ValidationDelegate());
  std::unique_ptr<ValidatingTextfield> textfield(
      new ValidatingTextfield(std::move(delegate)));

  // Set an invalid string (>5 characters).
  textfield->SetText(base::ASCIIToUTF16("evilstring"));
  // This should be called on new contents by the textfield controller.
  textfield->OnContentsChanged();

  // Not marked as invalid.
  EXPECT_FALSE(textfield->invalid());

  // On blur though, there is a first validation.
  textfield->OnBlur();
  EXPECT_TRUE(textfield->invalid());

  // On further text adjustements, the validation runs now. Set a valid string
  // (<=5 characters).
  textfield->SetText(base::ASCIIToUTF16("good"));
  textfield->OnContentsChanged();

  // No longer invalid.
  EXPECT_FALSE(textfield->invalid());
}

}  // namespace payments
