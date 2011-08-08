// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/phone_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

class PhoneFieldTest : public testing::Test {
 public:
  PhoneFieldTest() {}

 protected:
  ScopedVector<const AutofillField> list_;
  scoped_ptr<PhoneField> field_;
  FieldTypeMap field_type_map_;

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
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("phone1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ParseTwoLinePhone) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Area Code"),
                                               ASCIIToUTF16("area code"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("phone1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumber) {
  // Phone in format <field> - <field> - <field> could be either
  // <area code> - <prefix> - <suffix>, or
  // <country code> - <area code> - <phone>. The only distinguishing feature is
  // size: <prefix> is no bigger than 3 characters, and <suffix> is no bigger
  // than 4.
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Phone:"),
                                               ASCIIToUTF16("dayphone1"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("-"),
                                               ASCIIToUTF16("dayphone2"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               3,
                                               false),
                        ASCIIToUTF16("prefix1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("-"),
                                               ASCIIToUTF16("dayphone3"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               4,
                                               false),
                        ASCIIToUTF16("suffix1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("ext.:"),
                                               ASCIIToUTF16("dayphone4"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("ext1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("prefix1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("prefix1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("suffix1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("suffix1")]);
}

// This scenario of explicitly labeled "prefix" and "suffix" phone numbers
// encountered in http://crbug.com/40694 with page
// https://www.wrapables.com/jsp/Signup.jsp.
TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Phone:"),
                                               ASCIIToUTF16("area"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("prefix"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("prefix1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("suffix"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("suffix1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("prefix1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("prefix1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("suffix1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("suffix1")]);
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix2) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("("),
                                               ASCIIToUTF16("phone1"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               3,
                                               false),
                        ASCIIToUTF16("phone1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16(")"),
                                               ASCIIToUTF16("phone2"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               3,
                                               false),
                        ASCIIToUTF16("phone2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("phone3"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               4,
                                               false),
                        ASCIIToUTF16("phone3")));
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

TEST_F(PhoneFieldTest, ParseOneLineFax) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("fax"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("fax1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("fax1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, field_type_map_[ASCIIToUTF16("fax1")]);
}

TEST_F(PhoneFieldTest, ParseTwoLineFax) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Area Code"),
                                               ASCIIToUTF16("area code"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("fax"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("fax1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  // It should be FAX, based on the other phone in the group.
  EXPECT_EQ(PHONE_FAX_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("fax1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_FAX_NUMBER, field_type_map_[ASCIIToUTF16("fax1")]);
}

TEST_F(PhoneFieldTest, ThreePartFaxNumberPrefixSuffix) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Fax:"),
                                               ASCIIToUTF16("area"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("prefix"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("prefix1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("suffix"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("suffix1")));
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_FAX_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("prefix1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_FAX_NUMBER, field_type_map_[ASCIIToUTF16("prefix1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("suffix1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_FAX_NUMBER, field_type_map_[ASCIIToUTF16("suffix1")]);
}

TEST_F(PhoneFieldTest, CountryAndCityAndPhoneNumber) {
  // Phone in format <country code>:3 - <city and number>:10
  // The |maxlength| is considered, otherwise it's too broad.
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Phone Number"),
                                               ASCIIToUTF16("CountryCode"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               3,
                                               false),
                        ASCIIToUTF16("country")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Phone Number"),
                                               ASCIIToUTF16("PhoneNumber"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               10,
                                               false),
                        ASCIIToUTF16("phone")));
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
