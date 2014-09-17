// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/credit_card_field.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class CreditCardFieldTest : public testing::Test {
 public:
  CreditCardFieldTest() {}
  virtual ~CreditCardFieldTest() {}

 protected:
  ScopedVector<AutofillField> list_;
  scoped_ptr<const CreditCardField> field_;
  ServerFieldTypeMap field_type_map_;

  // Parses the contents of |list_| as a form, and stores the result into
  // |field_|.
  void Parse() {
    AutofillScanner scanner(list_.get());
    field_.reset(
        static_cast<const CreditCardField*>(CreditCardField::Parse(&scanner)));
  }

  // Associates fields with their corresponding types, based on the previous
  // call to Parse().
  bool ClassifyField() {
    return field_->ClassifyField(&field_type_map_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardFieldTest);
};

TEST_F(CreditCardFieldTest, Empty) {
  Parse();
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  Parse();
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

  Parse();
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoDate) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  Parse();
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
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

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
}

// Verifies that <input type="month"> controls are able to be parsed correctly.
TEST_F(CreditCardFieldTest, ParseMonthControl) {
  FormFieldData field;

  field.form_control_type = "text";
  field.label = ASCIIToUTF16("Card number:");
  field.name = ASCIIToUTF16("ccnumber");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  field.form_control_type = "month";
  field.label = ASCIIToUTF16("Expiration date:");
  field.name = ASCIIToUTF16("ccexp");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("date2")));

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("date2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("date2")]);
}

// Verify that heuristics <input name="ccyear" maxlength="2"/> considers
// *maxlength* attribute while parsing 2 Digit expiration year.
TEST_F(CreditCardFieldTest, ParseCreditCardExpYear_2DigitMaxLength) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number")));

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month")));

  field.name = ASCIIToUTF16("ccyear");
  field.max_length = 2;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year")));

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number")]);
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("month")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month")]);
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("year")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_2_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("year")]);
}

TEST_F(CreditCardFieldTest, ParseCreditCardNumberWithSplit) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q1");
  field.max_length = 4;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q2");
  field.max_length = 4;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q3");
  field.max_length = 4;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number3")));

  // For last credit card number input field it simply ignores the |max_length|
  // attribute. So even having a very big number, does not conside it an invalid
  // split for autofilling.
  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q4");
  field.max_length = 20;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number4")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month5")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year6")));

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());

  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number1")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  EXPECT_EQ(0U, list_[0]->credit_card_number_offset());

  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number2")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  EXPECT_EQ(4U, list_[1]->credit_card_number_offset());

  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number3")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number3")]);
  EXPECT_EQ(8U, list_[2]->credit_card_number_offset());

  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number4")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number4")]);
  EXPECT_EQ(12U, list_[3]->credit_card_number_offset());

  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("month5")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month5")]);
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("year6")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("year6")]);
}

TEST_F(CreditCardFieldTest, ParseCreditCardNumberWithInvalidSplit) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Not Card Number");
  field.name = ASCIIToUTF16("not_card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number3")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month4")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year5")));

  Parse();
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  EXPECT_TRUE(ClassifyField());

  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("name1")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number2")) ==
              field_type_map_.end());
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("number3")) ==
              field_type_map_.end());
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("month4")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month4")]);
  ASSERT_TRUE(field_type_map_.find(ASCIIToUTF16("year5")) !=
              field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("year5")]);
}

}  // namespace autofill
