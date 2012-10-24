// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/name_field.h"
#include "chrome/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class NameFieldTest : public testing::Test {
 public:
  NameFieldTest() {}

 protected:
  ScopedVector<const AutofillField> list_;
  scoped_ptr<NameField> field_;
  FieldTypeMap field_type_map_;

  // Downcast for tests.
  static NameField* Parse(AutofillScanner* scanner) {
    return static_cast<NameField*>(NameField::Parse(scanner));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NameFieldTest);
};

TEST_F(NameFieldTest, FirstMiddleLast) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("First");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Middle Name");
  field.name = ASCIIToUTF16("Middle");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("Last");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_MIDDLE, field_type_map_[ASCIIToUTF16("name2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name3")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name3")]);
}

TEST_F(NameFieldTest, FirstMiddleLast2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = string16();
  field.name = ASCIIToUTF16("firstName");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = string16();
  field.name = ASCIIToUTF16("middleName");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  field.label = string16();
  field.name = ASCIIToUTF16("lastName");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_MIDDLE, field_type_map_[ASCIIToUTF16("name2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name3")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name3")]);
}

TEST_F(NameFieldTest, FirstLast) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = string16();
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = string16();
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name2")]);
}

TEST_F(NameFieldTest, FirstLast2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name2")]);
}

TEST_F(NameFieldTest, FirstLastMiddleWithSpaces) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First  Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Middle  Name");
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Last  Name");
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_MIDDLE, field_type_map_[ASCIIToUTF16("name2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name3")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name3")]);
}

TEST_F(NameFieldTest, FirstLastEmpty) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

    field.label = string16();
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name2")]);
}

TEST_F(NameFieldTest, FirstMiddleLastEmpty) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = string16();
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  field.label = string16();
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_MIDDLE_INITIAL, field_type_map_[ASCIIToUTF16("name2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name3")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name3")]);
}

TEST_F(NameFieldTest, MiddleInitial) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("MI");
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_MIDDLE_INITIAL, field_type_map_[ASCIIToUTF16("name2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name3")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name3")]);
}

TEST_F(NameFieldTest, MiddleInitialNoLastName) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("MI");
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<NameField*>(NULL), field_.get());
}

// This case is from the dell.com checkout page.  The middle initial "mi" string
// came at the end following other descriptive text.  http://crbug.com/45123.
TEST_F(NameFieldTest, MiddleInitialAtEnd) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = string16();
  field.name = ASCIIToUTF16("XXXnameXXXfirst");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = string16();
  field.name = ASCIIToUTF16("XXXnameXXXmi");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name2")));

  field.label = string16();
  field.name = ASCIIToUTF16("XXXnameXXXlast");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_MIDDLE_INITIAL, field_type_map_[ASCIIToUTF16("name2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name3")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name3")]);
}
