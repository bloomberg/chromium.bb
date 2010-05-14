// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/name_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

namespace {

class NameFieldTest : public testing::Test {
 public:
  NameFieldTest() {}

 protected:
  ScopedVector<AutoFillField> list_;
  scoped_ptr<NameField> field_;
  FieldTypeMap field_type_map_;
  std::vector<AutoFillField*>::const_iterator iter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NameFieldTest);
};

TEST_F(NameFieldTest, FirstMiddleLast) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("First"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Middle Name"),
                                               ASCIIToUTF16("Middle"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("Last"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
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
      new AutoFillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("firstName"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("middleName"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("lastName"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
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
      new AutoFillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
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
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Name"),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
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
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("First  Name"),
                                               ASCIIToUTF16("first  name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name1")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Middle  Name"),
                                               ASCIIToUTF16("middle  name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("name2")));
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Last  Name"),
                                               ASCIIToUTF16("last  name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
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

}  // namespace
