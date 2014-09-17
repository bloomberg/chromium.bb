// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/phone_field.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class PhoneFieldTest : public testing::Test {
 public:
  PhoneFieldTest() {}

 protected:
  ScopedVector<AutofillField> list_;
  scoped_ptr<PhoneField> field_;
  ServerFieldTypeMap field_type_map_;

  // Downcast for tests.
  static PhoneField* Parse(AutofillScanner* scanner) {
    return static_cast<PhoneField*>(PhoneField::Parse(scanner));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PhoneFieldTest);
};

TEST_F(PhoneFieldTest, Empty) {
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<PhoneField*>(NULL), field_.get());
}

TEST_F(PhoneFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<PhoneField*>(NULL), field_.get());
}

TEST_F(PhoneFieldTest, ParseOneLinePhone) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("phone1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ParseTwoLinePhone) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Area Code");
  field.name = ASCIIToUTF16("area code");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("areacode1")));

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("phone2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone2")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone2")]);
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumber) {
  // Phone in format <field> - <field> - <field> could be either
  // <area code> - <prefix> - <suffix>, or
  // <country code> - <area code> - <phone>. The only distinguishing feature is
  // size: <prefix> is no bigger than 3 characters, and <suffix> is no bigger
  // than 4.
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Phone:");
  field.name = ASCIIToUTF16("dayphone1");
  field.max_length = 0;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("areacode1")));

  field.label = ASCIIToUTF16("-");
  field.name = ASCIIToUTF16("dayphone2");
  field.max_length = 3;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("prefix2")));

  field.label = ASCIIToUTF16("-");
  field.name = ASCIIToUTF16("dayphone3");
  field.max_length = 4;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("suffix3")));

  field.label = ASCIIToUTF16("ext.:");
  field.name = ASCIIToUTF16("dayphone4");
  field.max_length = 0;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("ext4")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("prefix2")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("prefix2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("suffix3")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("suffix3")]);
  EXPECT_TRUE(
      field_type_map_.find(ASCIIToUTF16("ext4")) == field_type_map_.end());
}

// This scenario of explicitly labeled "prefix" and "suffix" phone numbers
// encountered in http://crbug.com/40694 with page
// https://www.wrapables.com/jsp/Signup.jsp.
TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Phone:");
  field.name = ASCIIToUTF16("area");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("areacode1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("prefix");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("prefix2")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("suffix");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("suffix3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("prefix2")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("prefix2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("suffix3")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("suffix3")]);
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("(");
  field.name = ASCIIToUTF16("phone1");
  field.max_length = 3;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("phone1")));

  field.label = ASCIIToUTF16(")");
  field.name = ASCIIToUTF16("phone2");
  field.max_length = 3;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("phone2")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("phone3");
  field.max_length = 4;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("phone3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("phone1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone2")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone3")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone3")]);
}

TEST_F(PhoneFieldTest, CountryAndCityAndPhoneNumber) {
  // Phone in format <country code>:3 - <city and number>:10
  // The |maxlength| is considered, otherwise it's too broad.
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Phone Number");
  field.name = ASCIIToUTF16("CountryCode");
  field.max_length = 3;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("country")));

  field.label = ASCIIToUTF16("Phone Number");
  field.name = ASCIIToUTF16("PhoneNumber");
  field.max_length = 10;
  list_.push_back(new AutofillField(field, ASCIIToUTF16("phone")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("country")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_COUNTRY_CODE, field_type_map_[ASCIIToUTF16("country")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER, field_type_map_[ASCIIToUTF16("phone")]);
}

}  // namespace autofill
