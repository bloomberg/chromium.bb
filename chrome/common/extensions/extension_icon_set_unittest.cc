// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_icon_set.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionIconSet, Basic) {
  ExtensionIconSet icons;
  EXPECT_EQ("", icons.Get(42, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("", icons.Get(42, ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("", icons.Get(42, ExtensionIconSet::MATCH_SMALLER));
  EXPECT_TRUE(icons.map().empty());

  icons.Add(42, "42.png");
  EXPECT_EQ("42.png", icons.Get(42, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("42.png", icons.Get(42, ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("42.png", icons.Get(42, ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("42.png", icons.Get(41, ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("42.png", icons.Get(43, ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("", icons.Get(41, ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("", icons.Get(43, ExtensionIconSet::MATCH_BIGGER));

  icons.Add(38, "38.png");
  icons.Add(40, "40.png");
  icons.Add(44, "44.png");
  icons.Add(46, "46.png");

  EXPECT_EQ("", icons.Get(41, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("40.png", icons.Get(41, ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("42.png", icons.Get(41, ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("", icons.Get(37, ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("", icons.Get(47, ExtensionIconSet::MATCH_BIGGER));
}
