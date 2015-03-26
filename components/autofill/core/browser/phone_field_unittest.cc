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

namespace {

const char* const kFieldTypes[] = {
  "text",
  "tel",
  "number",
};

}  // namespace

class PhoneFieldTest : public testing::Test {
 public:
  PhoneFieldTest() {}

 protected:
  // Downcast for tests.
  static scoped_ptr<PhoneField> Parse(AutofillScanner* scanner) {
    scoped_ptr<FormField> field = PhoneField::Parse(scanner);
    return make_scoped_ptr(static_cast<PhoneField*>(field.release()));
  }

  void Clear() {
    list_.clear();
    field_.reset();
    field_type_map_.clear();
  }

  void CheckField(const std::string& name,
                  ServerFieldType expected_type) const {
    auto it = field_type_map_.find(ASCIIToUTF16(name));
    ASSERT_TRUE(it != field_type_map_.end()) << name;
    EXPECT_EQ(expected_type, it->second) << name;
  }

  ScopedVector<AutofillField> list_;
  scoped_ptr<PhoneField> field_;
  ServerFieldTypeMap field_type_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhoneFieldTest);
};

TEST_F(PhoneFieldTest, Empty) {
  AutofillScanner scanner(list_.get());
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(PhoneFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  AutofillScanner scanner(list_.get());
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(PhoneFieldTest, ParseOneLinePhone) {
  FormFieldData field;

  for (size_t i = 0; i < arraysize(kFieldTypes); ++i) {
    Clear();

    field.form_control_type = kFieldTypes[i];
    field.label = ASCIIToUTF16("Phone");
    field.name = ASCIIToUTF16("phone");
    list_.push_back(new AutofillField(field, ASCIIToUTF16("phone1")));

    AutofillScanner scanner(list_.get());
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
    CheckField("phone1", PHONE_HOME_WHOLE_NUMBER);
  }
}

TEST_F(PhoneFieldTest, ParseTwoLinePhone) {
  FormFieldData field;

  for (size_t i = 0; i < arraysize(kFieldTypes); ++i) {
    Clear();

    field.form_control_type = kFieldTypes[i];
    field.label = ASCIIToUTF16("Area Code");
    field.name = ASCIIToUTF16("area code");
    list_.push_back(new AutofillField(field, ASCIIToUTF16("areacode1")));

    field.label = ASCIIToUTF16("Phone");
    field.name = ASCIIToUTF16("phone");
    list_.push_back(new AutofillField(field, ASCIIToUTF16("phone2")));

    AutofillScanner scanner(list_.get());
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
    CheckField("areacode1", PHONE_HOME_CITY_CODE);
    CheckField("phone2", PHONE_HOME_NUMBER);
  }
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumber) {
  // Phone in format <field> - <field> - <field> could be either
  // <area code> - <prefix> - <suffix>, or
  // <country code> - <area code> - <phone>. The only distinguishing feature is
  // size: <prefix> is no bigger than 3 characters, and <suffix> is no bigger
  // than 4.
  FormFieldData field;

  for (size_t i = 0; i < arraysize(kFieldTypes); ++i) {
    Clear();

    field.form_control_type = kFieldTypes[i];
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
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
    CheckField("areacode1", PHONE_HOME_CITY_CODE);
    CheckField("prefix2", PHONE_HOME_NUMBER);
    CheckField("suffix3", PHONE_HOME_NUMBER);
    EXPECT_FALSE(ContainsKey(field_type_map_, ASCIIToUTF16("ext4")));
  }
}

// This scenario of explicitly labeled "prefix" and "suffix" phone numbers
// encountered in http://crbug.com/40694 with page
// https://www.wrapables.com/jsp/Signup.jsp.
TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix) {
  FormFieldData field;

  for (size_t i = 0; i < arraysize(kFieldTypes); ++i) {
    Clear();

    field.form_control_type = kFieldTypes[i];
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
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
    CheckField("areacode1", PHONE_HOME_CITY_CODE);
    CheckField("prefix2", PHONE_HOME_NUMBER);
    CheckField("suffix3", PHONE_HOME_NUMBER);
  }
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix2) {
  FormFieldData field;

  for (size_t i = 0; i < arraysize(kFieldTypes); ++i) {
    Clear();

    field.form_control_type = kFieldTypes[i];
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
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
    CheckField("phone1", PHONE_HOME_CITY_CODE);
    CheckField("phone2", PHONE_HOME_NUMBER);
    CheckField("phone3", PHONE_HOME_NUMBER);
  }
}

TEST_F(PhoneFieldTest, CountryAndCityAndPhoneNumber) {
  // Phone in format <country code>:3 - <city and number>:10
  // The |maxlength| is considered, otherwise it's too broad.
  FormFieldData field;

  for (size_t i = 0; i < arraysize(kFieldTypes); ++i) {
    Clear();

    field.form_control_type = kFieldTypes[i];
    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("CountryCode");
    field.max_length = 3;
    list_.push_back(new AutofillField(field, ASCIIToUTF16("country")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("PhoneNumber");
    field.max_length = 10;
    list_.push_back(new AutofillField(field, ASCIIToUTF16("phone")));

    AutofillScanner scanner(list_.get());
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
    CheckField("country", PHONE_HOME_COUNTRY_CODE);
    CheckField("phone", PHONE_HOME_CITY_AND_NUMBER);
  }
}

}  // namespace autofill
