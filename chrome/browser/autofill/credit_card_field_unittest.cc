// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/credit_card_field.h"
#include "chrome/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class CreditCardFieldTest : public testing::Test {
 public:
  CreditCardFieldTest() {}

 protected:
  ScopedVector<const AutofillField> list_;
  scoped_ptr<CreditCardField> field_;
  FieldTypeMap field_type_map_;

  // Downcast for tests.
  static CreditCardField* Parse(AutofillScanner* scanner) {
    return static_cast<CreditCardField*>(CreditCardField::Parse(scanner));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardFieldTest);
};

TEST_F(CreditCardFieldTest, Empty) {
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoNumber) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month1")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoDate) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseMiniumCreditCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month2")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year3")]);
}

TEST_F(CreditCardFieldTest, ParseFullCreditCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Type");
  field.name = ASCIIToUTF16("card_type");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("type")));

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("cvc")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("type")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_TYPE, field_type_map_[ASCIIToUTF16("type")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("cvc")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
      field_type_map_[ASCIIToUTF16("cvc")]);
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year4")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month3")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year4")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year4")]);
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year4")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month3")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year4")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year4")]);
}

TEST_F(CreditCardFieldTest, ParseExpField) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration Date (MM/YYYY)");
  field.name = ASCIIToUTF16("cc_exp");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("exp3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("exp3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("exp3")]);
}

TEST_F(CreditCardFieldTest, ParseExpField2DigitYear) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration Date (MM/YY)");
  field.name = ASCIIToUTF16("cc_exp");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("exp3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("exp3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("exp3")]);
}

TEST_F(CreditCardFieldTest, ParseCreditCardHolderNameWithCCFullName) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("ccfullname");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
}
