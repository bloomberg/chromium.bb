// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address_field.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class AddressFieldTest : public testing::Test {
 public:
  AddressFieldTest() {}

 protected:
  ScopedVector<const AutofillField> list_;
  scoped_ptr<AddressField> field_;
  FieldTypeMap field_type_map_;

  // Downcast for tests.
  static AddressField* Parse(AutofillScanner* scanner) {
    return static_cast<AddressField*>(AddressField::Parse(scanner));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressFieldTest);
};

TEST_F(AddressFieldTest, Empty) {
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<AddressField*>(NULL), field_.get());
}

TEST_F(AddressFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<AddressField*>(NULL), field_.get());
}

TEST_F(AddressFieldTest, ParseOneLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
}

TEST_F(AddressFieldTest, ParseOneLineAddressBilling) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("billingAddress");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kBillingAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_BILLING_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
}

TEST_F(AddressFieldTest, ParseOneLineAddressShipping) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("shippingAddress");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kShippingAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
}

TEST_F(AddressFieldTest, ParseTwoLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr1")));

  field.label = string16();
  field.name = string16();
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr2")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2, field_type_map_[ASCIIToUTF16("addr2")]);
}

TEST_F(AddressFieldTest, ParseThreeLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address1");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr1")));

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address2");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr2")));

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("Address3");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr2")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2, field_type_map_[ASCIIToUTF16("addr2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr3")) == field_type_map_.end());
}

TEST_F(AddressFieldTest, ParseCity) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("city1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("city1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY, field_type_map_[ASCIIToUTF16("city1")]);
}

TEST_F(AddressFieldTest, ParseState) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("state1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("state1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE, field_type_map_[ASCIIToUTF16("state1")]);
}

TEST_F(AddressFieldTest, ParseZip) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Zip");
  field.name = ASCIIToUTF16("zip");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("zip1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("zip1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_type_map_[ASCIIToUTF16("zip1")]);
}

TEST_F(AddressFieldTest, ParseStateAndZipOneLabel) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("State/Province, Zip/Postal Code");
  field.name = ASCIIToUTF16("state");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("state")));

  field.label = ASCIIToUTF16("State/Province, Zip/Postal Code");
  field.name = ASCIIToUTF16("zip");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("zip")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("state")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE, field_type_map_[ASCIIToUTF16("state")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("zip")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP, field_type_map_[ASCIIToUTF16("zip")]);
}

TEST_F(AddressFieldTest, ParseCountry) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("country1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("country1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, field_type_map_[ASCIIToUTF16("country1")]);
}

TEST_F(AddressFieldTest, ParseTwoLineAddressMissingLabel) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr1")));

  field.label = string16();
  field.name = ASCIIToUTF16("bogus");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("addr2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr1")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1, field_type_map_[ASCIIToUTF16("addr1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("addr2")) != field_type_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2, field_type_map_[ASCIIToUTF16("addr2")]);
}

TEST_F(AddressFieldTest, ParseCompany) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Company");
  field.name = ASCIIToUTF16("company");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("company1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<AddressField*>(NULL), field_.get());
  EXPECT_EQ(AddressField::kGenericAddress, field_->FindType());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("company1")) != field_type_map_.end());
  EXPECT_EQ(COMPANY_NAME, field_type_map_[ASCIIToUTF16("company1")]);
}
