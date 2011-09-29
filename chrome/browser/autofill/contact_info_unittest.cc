// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/contact_info.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NameInfoTest, SetFullName) {
  NameInfo name;
  name.SetInfo(NAME_FULL, ASCIIToUTF16("Virgil"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("Virgil"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), string16());
  EXPECT_EQ(name.GetInfo(NAME_LAST), string16());
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("Virgil"));

  name.SetInfo(NAME_FULL, ASCIIToUTF16("Murray Gell-Mann"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("Murray"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), string16());
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Gell-Mann"));
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("Murray Gell-Mann"));

  name.SetInfo(NAME_FULL,
               ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("Mikhail"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), ASCIIToUTF16("Yevgrafovich"));
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Saltykov-Shchedrin"));
  EXPECT_EQ(name.GetInfo(NAME_FULL),
            ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));

  name.SetInfo(NAME_FULL, ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("Arthur"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), ASCIIToUTF16("Ignatius Conan"));
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Doyle"));
  EXPECT_EQ(name.GetInfo(NAME_FULL),
            ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
}

TEST(NameInfoTest, GetFullName) {
  NameInfo name;
  name.SetInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetInfo(NAME_MIDDLE, string16());
  name.SetInfo(NAME_LAST, string16());
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), string16());
  EXPECT_EQ(name.GetInfo(NAME_LAST), string16());
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("First"));

  name.SetInfo(NAME_FIRST, string16());
  name.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetInfo(NAME_LAST, string16());
  EXPECT_EQ(name.GetInfo(NAME_FIRST), string16());
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetInfo(NAME_LAST), string16());
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("Middle"));

  name.SetInfo(NAME_FIRST, string16());
  name.SetInfo(NAME_MIDDLE, string16());
  name.SetInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), string16());
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), string16());
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("Last"));

  name.SetInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetInfo(NAME_LAST, string16());
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetInfo(NAME_LAST), string16());
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("First Middle"));

  name.SetInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetInfo(NAME_MIDDLE, string16());
  name.SetInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), string16());
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("First Last"));

  name.SetInfo(NAME_FIRST, string16());
  name.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), string16());
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("Middle Last"));

  name.SetInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetInfo(NAME_FULL), ASCIIToUTF16("First Middle Last"));
}

