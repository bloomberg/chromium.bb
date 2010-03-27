// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/address_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_field_values.h"

namespace {

class AddressFieldTest : public testing::Test {
 public:
  AddressFieldTest() {}

 protected:
  ScopedVector<AutoFillField> list_;
  scoped_ptr<AddressField> field_;
  FieldTypeMap field_type_map_;
  std::vector<AutoFillField*>::const_iterator iter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressFieldTest);
};

TEST_F(AddressFieldTest, Empty) {
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<AddressField*>(NULL), field_.get());
}

TEST_F(AddressFieldTest, NonParse) {
  list_.push_back(new AutoFillField);
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<AddressField*>(NULL), field_.get());
}

TEST_F(AddressFieldTest, ParseOneLineAddress) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("addr1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kShippingAddress, field_->FindType());
  EXPECT_TRUE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
}

TEST_F(AddressFieldTest, ParseOneLineAddressEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               kEcmlShipToAddress1,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("addr1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, true));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kShippingAddress, field_->FindType());
  EXPECT_TRUE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
}

TEST_F(AddressFieldTest, ParseTwoLineAddress) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("addr1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(string16(),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("addr2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kShippingAddress, field_->FindType());
  EXPECT_TRUE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr2")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2, field_type_map_[ASCIIToUTF16("addr2")]);
}

TEST_F(AddressFieldTest, ParseTwoLineAddressEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               kEcmlShipToAddress1,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("addr1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(string16(),
                                               kEcmlShipToAddress2,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("addr2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, true));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kShippingAddress, field_->FindType());
  EXPECT_TRUE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr2")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2, field_type_map_[ASCIIToUTF16("addr2")]);
}

TEST_F(AddressFieldTest, ParseCity) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("City"),
                                               ASCIIToUTF16("city"),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("city1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("city1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY, field_type_map_[ASCIIToUTF16("city1")]);
}

TEST_F(AddressFieldTest, ParseCityEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("City"),
                                               kEcmlShipToCity,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("city1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, true));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("city1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY, field_type_map_[ASCIIToUTF16("city1")]);
}

TEST_F(AddressFieldTest, ParseState) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("State"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("state1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("state1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE, field_type_map_[ASCIIToUTF16("state1")]);
}

TEST_F(AddressFieldTest, ParseStateEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("State"),
                                               kEcmlShipToStateProv,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("state1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, true));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("state1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE, field_type_map_[ASCIIToUTF16("state1")]);
}

TEST_F(AddressFieldTest, ParseZip) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Zip"),
                                               ASCIIToUTF16("zip"),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("zip1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("zip1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_type_map_[ASCIIToUTF16("zip1")]);
}

TEST_F(AddressFieldTest, ParseZipEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Zip"),
                                               kEcmlShipToPostalCode,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("zip1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, true));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("zip1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_type_map_[ASCIIToUTF16("zip1")]);
}

TEST_F(AddressFieldTest, ParseCountry) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Country"),
                                               ASCIIToUTF16("country"),
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("country1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, false));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("country1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, field_type_map_[ASCIIToUTF16("country1")]);
}

TEST_F(AddressFieldTest, ParseCountryEcml) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Country"),
                                               kEcmlShipToCountry,
                                               string16(),
                                               ASCIIToUTF16("text")),
                        ASCIIToUTF16("country1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(AddressField::Parse(&iter_, true));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(kGenericAddress, field_->FindType());
  EXPECT_FALSE(field_->IsFullAddress());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("country1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, field_type_map_[ASCIIToUTF16("country1")]);
}

}  // namespace
