// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/contact_info.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NameInfoTest, TestSetFullName) {
  NameInfo name;
  name.SetFullName(ASCIIToUTF16("Virgil"));
  EXPECT_EQ(name.first(), ASCIIToUTF16("Virgil"));
  EXPECT_EQ(name.middle(), ASCIIToUTF16(""));
  EXPECT_EQ(name.last(), ASCIIToUTF16(""));
  EXPECT_EQ(name.FullName(), ASCIIToUTF16("Virgil"));

  name.SetFullName(ASCIIToUTF16("Murray Gell-Mann"));
  EXPECT_EQ(name.first(), ASCIIToUTF16("Murray"));
  EXPECT_EQ(name.middle(), ASCIIToUTF16(""));
  EXPECT_EQ(name.last(), ASCIIToUTF16("Gell-Mann"));
  EXPECT_EQ(name.FullName(), ASCIIToUTF16("Murray Gell-Mann"));

  name.SetFullName(ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));
  EXPECT_EQ(name.first(), ASCIIToUTF16("Mikhail"));
  EXPECT_EQ(name.middle(), ASCIIToUTF16("Yevgrafovich"));
  EXPECT_EQ(name.last(), ASCIIToUTF16("Saltykov-Shchedrin"));
  EXPECT_EQ(name.FullName(),
      ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));

  name.SetFullName(ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
  EXPECT_EQ(name.first(), ASCIIToUTF16("Arthur"));
  EXPECT_EQ(name.middle(), ASCIIToUTF16("Ignatius Conan"));
  EXPECT_EQ(name.last(), ASCIIToUTF16("Doyle"));
  EXPECT_EQ(name.FullName(), ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
}

