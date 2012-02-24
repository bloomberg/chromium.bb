// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_icon_set.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionIconSet, Basic) {
  ExtensionIconSet icons;
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_SMALLER));
  EXPECT_TRUE(icons.map().empty());

  icons.Add(ExtensionIconSet::EXTENSION_ICON_LARGE, "large.png");
  EXPECT_EQ("large.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("large.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("large.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                   ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("large.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("large.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_EXTRA_LARGE,
                                   ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("large.png", icons.Get(0, ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_EXTRA_LARGE,
                          ExtensionIconSet::MATCH_BIGGER));

  icons.Add(ExtensionIconSet::EXTENSION_ICON_SMALLISH, "smallish.png");
  icons.Add(ExtensionIconSet::EXTENSION_ICON_SMALL, "small.png");
  icons.Add(ExtensionIconSet::EXTENSION_ICON_EXTRA_LARGE, "extra_large.png");

  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_MEDIUM,
                          ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("small.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("large.png", icons.Get(ExtensionIconSet::EXTENSION_ICON_MEDIUM,
                                   ExtensionIconSet::MATCH_BIGGER));
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_BITTY,
                          ExtensionIconSet::MATCH_SMALLER));
  EXPECT_EQ("", icons.Get(ExtensionIconSet::EXTENSION_ICON_GIGANTOR,
                          ExtensionIconSet::MATCH_BIGGER));
}

TEST(ExtensionIconSet, Values) {
  ExtensionIconSet icons;
  EXPECT_FALSE(icons.ContainsPath("foo"));

  icons.Add(ExtensionIconSet::EXTENSION_ICON_BITTY, "foo");
  icons.Add(ExtensionIconSet::EXTENSION_ICON_GIGANTOR, "bar");

  EXPECT_TRUE(icons.ContainsPath("foo"));
  EXPECT_TRUE(icons.ContainsPath("bar"));
  EXPECT_FALSE(icons.ContainsPath("baz"));
  EXPECT_FALSE(icons.ContainsPath(""));

  icons.Clear();
  EXPECT_FALSE(icons.ContainsPath("foo"));
}

TEST(ExtensionIconSet, FindSize) {
  ExtensionIconSet icons;
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath("foo"));

  icons.Add(ExtensionIconSet::EXTENSION_ICON_BITTY, "foo");
  icons.Add(ExtensionIconSet::EXTENSION_ICON_GIGANTOR, "bar");

  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_BITTY,
            icons.GetIconSizeFromPath("foo"));
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_GIGANTOR,
            icons.GetIconSizeFromPath("bar"));
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath("baz"));
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath(""));

  icons.Clear();
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_INVALID,
            icons.GetIconSizeFromPath("foo"));
}
