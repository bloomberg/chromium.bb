// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/credit_card_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

namespace {

class CreditCardFieldTest : public testing::Test {
 public:
  CreditCardFieldTest() {}

 protected:
  ScopedVector<AutofillField> list_;
  scoped_ptr<CreditCardField> field_;
  FieldTypeMap field_type_map_;
  std::vector<AutofillField*>::const_iterator iter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardFieldTest);
};

TEST_F(CreditCardFieldTest, Empty) {
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoNumber) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("month1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("year1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoDate) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseMiniumCreditCard) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("month1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("year1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year1")]);
}

TEST_F(CreditCardFieldTest, ParseMiniumCreditCardEcml) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16(kEcmlCardNumber),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number1")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                 ASCIIToUTF16(kEcmlCardExpireMonth),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("month1")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                 ASCIIToUTF16(kEcmlCardExpireYear),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("year1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year1")]);
}

TEST_F(CreditCardFieldTest, ParseFullCreditCard) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name on card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("month1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("year1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Verification"),
                                               ASCIIToUTF16("verification"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("cvc1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year1")]);
  // We don't store CVV.
  EXPECT_TRUE(
      field_type_map_.find(ASCIIToUTF16("cvc1")) == field_type_map_.end());
}

TEST_F(CreditCardFieldTest, ParseFullCreditCardEcml) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16(kEcmlCardHolder),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16(kEcmlCardNumber),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number1")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                 ASCIIToUTF16(kEcmlCardExpireMonth),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("month1")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                 ASCIIToUTF16(kEcmlCardExpireYear),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("year1")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Verification"),
                                 ASCIIToUTF16(kEcmlCardVerification),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("cvc1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year1")]);
  // We don't store the CVV.
  EXPECT_TRUE(
      field_type_map_.find(ASCIIToUTF16("cvc1")) == field_type_map_.end());
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("Name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("Card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("ExpDate Month / Year"),
                                 ASCIIToUTF16("ExpDate"),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("month")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("ExpDate Month / Year"),
                                 ASCIIToUTF16("ExpDate"),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("year")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear2) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("Name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("Card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("number")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Expiration date Month / Year"),
                                 ASCIIToUTF16("ExpDate"),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("month")));
  list_.push_back(
      new AutofillField(
          webkit_glue::FormField(ASCIIToUTF16("Expiration date Month / Year"),
                                 ASCIIToUTF16("ExpDate"),
                                 string16(),
                                 ASCIIToUTF16("text"),
                                 0,
                                 false),
          ASCIIToUTF16("year")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(CreditCardField::Parse(&iter_, false));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
}

}  // namespace

