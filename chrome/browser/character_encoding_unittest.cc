// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/character_encoding.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(CharacterEncodingTest, GetCanonicalEncodingNameByAliasName) {
  EXPECT_EQ("Big5",
            GetCanonicalEncodingNameByAliasName("Big5"));
  EXPECT_EQ("windows-874",
            GetCanonicalEncodingNameByAliasName("windows-874"));
  EXPECT_EQ("ISO-8859-8",
            GetCanonicalEncodingNameByAliasName("ISO-8859-8"));

  // Non-canonical alias names should be converted to a canonical one.
  EXPECT_EQ("UTF-8",
            GetCanonicalEncodingNameByAliasName("utf8"));
  EXPECT_EQ("gb18030",
            GetCanonicalEncodingNameByAliasName("GB18030"));
}
