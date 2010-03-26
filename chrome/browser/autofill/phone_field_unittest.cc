// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/phone_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_field_values.h"

namespace {

class PhoneFieldTest : public testing::Test {
 public:
  PhoneFieldTest() {}

 protected:
  ScopedVector<AutoFillField> list_;
  scoped_ptr<PhoneField> field_;
  FieldTypeMap field_type_map_;
  std::vector<AutoFillField*>::const_iterator iter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhoneFieldTest);
};

TEST_F(PhoneFieldTest, Empty) {
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<PhoneField*>(NULL), field_.get());
}

TEST_F(PhoneFieldTest, NonParse) {
  list_.push_back(new AutoFillField);
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<PhoneField*>(NULL), field_.get());
}

TEST_F(PhoneFieldTest, ParseOneLinePhone) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("phone1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, false));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ParseOneLinePhoneEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               kEcmlShipToPhone,
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("phone1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, true));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ParseTwoLinePhone) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Area Code"),
                                               ASCIIToUTF16("area code"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("phone1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, false));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ParseTwoLinePhoneEcmlShipTo) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Area Code"),
                                               kEcmlShipToPostalCode,
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               kEcmlShipToPhone,
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("phone1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, false));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

TEST_F(PhoneFieldTest, ParseTwoLinePhoneEcmlBillTo) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Area Code"),
                                               kEcmlBillToPostalCode,
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("areacode1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               kEcmlBillToPhone,
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               WebKit::WebInputElement::Text),
                        ASCIIToUTF16("phone1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(PhoneField::Parse(&iter_, false));
  ASSERT_NE(static_cast<PhoneField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("areacode1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_CITY_CODE, field_type_map_[ASCIIToUTF16("areacode1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("phone1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_HOME_NUMBER, field_type_map_[ASCIIToUTF16("phone1")]);
}

}  // namespace
