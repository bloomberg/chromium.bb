// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/fax_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

namespace {

class FaxFieldTest : public testing::Test {
 public:
  FaxFieldTest() {}

 protected:
  ScopedVector<AutoFillField> list_;
  scoped_ptr<FaxField> field_;
  FieldTypeMap field_type_map_;
  std::vector<AutoFillField*>::const_iterator iter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FaxFieldTest);
};

TEST_F(FaxFieldTest, Empty) {
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(FaxField::Parse(&iter_));
  ASSERT_EQ(static_cast<FaxField*>(NULL), field_.get());
}

TEST_F(FaxFieldTest, NonParse) {
  list_.push_back(new AutoFillField);
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(FaxField::Parse(&iter_));
  ASSERT_EQ(static_cast<FaxField*>(NULL), field_.get());
}

TEST_F(FaxFieldTest, ParseOneLineFax) {
  list_.push_back(
      new AutoFillField(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("faxnumber"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0),
                        ASCIIToUTF16("fax1")));
  list_.push_back(NULL);
  iter_ = list_.begin();
  field_.reset(FaxField::Parse(&iter_));
  ASSERT_NE(static_cast<FaxField*>(NULL), field_.get());
  ASSERT_TRUE(field_->GetFieldInfo(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("fax1")) != field_type_map_.end());
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, field_type_map_[ASCIIToUTF16("fax1")]);
}

}  // namespace
