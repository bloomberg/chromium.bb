// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"

using browser_sync::AutofillModelAssociator;

class AutofillModelAssociatorTest : public testing::Test {
};

TEST_F(AutofillModelAssociatorTest, KeyToTag) {
  EXPECT_EQ("autofill_entry|foo|bar",
            AutofillModelAssociator::KeyToTag(UTF8ToUTF16("foo"),
                                              UTF8ToUTF16("bar")));
  EXPECT_EQ("autofill_entry|%7C|%7C",
            AutofillModelAssociator::KeyToTag(UTF8ToUTF16("|"),
                                              UTF8ToUTF16("|")));
  EXPECT_EQ("autofill_entry|%7C|",
            AutofillModelAssociator::KeyToTag(UTF8ToUTF16("|"),
                                              UTF8ToUTF16("")));
  EXPECT_EQ("autofill_entry||%7C",
            AutofillModelAssociator::KeyToTag(UTF8ToUTF16(""),
                                              UTF8ToUTF16("|")));
}

TEST_F(AutofillModelAssociatorTest, ProfileLabelToTag) {
  string16 label(ASCIIToUTF16("awesome_address"));
  EXPECT_EQ("autofill_profile|awesome_address",
            AutofillModelAssociator::ProfileLabelToTag(label));

  EXPECT_EQ("autofill_profile|%7C%7C",
            AutofillModelAssociator::ProfileLabelToTag(ASCIIToUTF16("||")));
  EXPECT_NE(AutofillModelAssociator::KeyToTag(ASCIIToUTF16("autofill_profile"),
                                              ASCIIToUTF16("home")),
            AutofillModelAssociator::ProfileLabelToTag(ASCIIToUTF16("home")));
}
