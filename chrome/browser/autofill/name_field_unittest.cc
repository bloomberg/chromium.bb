// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/name_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

namespace {

class NameFieldTest : public testing::Test {
 public:
  NameFieldTest() {}

 protected:
  ScopedVector<AutofillField> list_;
  scoped_ptr<NameField> field_;
  FieldTypeMap field_type_map_;
  std::vector<AutofillField*>::const_iterator iter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NameFieldTest);
};

TEST_F(NameFieldTest, FirstMiddleLast) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("First"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Middle Name"),
                                               ASCIIToUTF16("Middle"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("Last"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("firstName"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("middleName"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("lastName"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name2")]);
}

TEST_F(NameFieldTest, FirstLast2) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name"),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name2")]);
}

TEST_F(NameFieldTest, FirstLastMiddleWithSpaces) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("First  Name"),
                                               ASCIIToUTF16("first  name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Middle  Name"),
                                               ASCIIToUTF16("middle  name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Last  Name"),
                                               ASCIIToUTF16("last  name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(NAME_FIRST, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name2")) != field_type_map_.end());
  EXPECT_EQ(NAME_LAST, field_type_map_[ASCIIToUTF16("name2")]);
}

TEST_F(NameFieldTest, FirstMiddleLastEmpty) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("middle_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("MI"),
                                               ASCIIToUTF16("middle_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(ASCIIToUTF16("MI"),
                                               ASCIIToUTF16("middle_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_EQ(static_cast<NameField*>(NULL), field_.get());
}

// This case is from the dell.com checkout page.  The middle initial "mi" string
// came at the end following other descriptive text.  http://crbug.com/45123.
TEST_F(NameFieldTest, MiddleInitialAtEnd) {
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("XXXnameXXXfirst"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("XXXnameXXXmi"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutofillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("XXXnameXXXlast"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false),
                        ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, false));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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

TEST_F(NameFieldTest, ECMLNoName) {
  list_.push_back(new AutofillField(
      webkit_glue::FormField(ASCIIToUTF16("Company"),
                             ASCIIToUTF16("ecom_shipto_postal_company"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false),
      ASCIIToUTF16("field1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, true));
  ASSERT_EQ(static_cast<NameField*>(NULL), field_.get());
}

TEST_F(NameFieldTest, ECMLMiddleInitialNoLastName) {
  list_.push_back(new AutofillField(
      webkit_glue::FormField(ASCIIToUTF16("First Name"),
                             ASCIIToUTF16("ecom_shipto_postal_name_first"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false),
      ASCIIToUTF16("name1")));
  list_.push_back(new AutofillField(
      webkit_glue::FormField(ASCIIToUTF16("Middle"),
                             ASCIIToUTF16("ecom_shipto_postal_name_middle"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false),
      ASCIIToUTF16("name2")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, true));
  ASSERT_EQ(static_cast<NameField*>(NULL), field_.get());
}

TEST_F(NameFieldTest, ECMLFirstMiddleLast) {
  list_.push_back(new AutofillField(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("ecom_shipto_postal_name_first"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false),
      ASCIIToUTF16("name1")));
  list_.push_back(new AutofillField(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("ecom_shipto_postal_name_middle"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false),
      ASCIIToUTF16("name2")));
  list_.push_back(new AutofillField(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("ecom_shipto_postal_name_last"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false),
      ASCIIToUTF16("name3")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(NameField::Parse(&iter_, true));
  ASSERT_NE(static_cast<NameField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
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

}  // namespace
