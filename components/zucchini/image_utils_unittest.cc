// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/image_utils.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(ImageUtilsTest, Bitness) {
  EXPECT_EQ(4U, WidthOf(kBit32));
  EXPECT_EQ(8U, WidthOf(kBit64));
}

TEST(ImageUtilsTest, IsMarked) {
  EXPECT_FALSE(IsMarked(0x00000000));
  EXPECT_TRUE(IsMarked(0x80000000));

  EXPECT_FALSE(IsMarked(0x00000001));
  EXPECT_TRUE(IsMarked(0x80000001));

  EXPECT_FALSE(IsMarked(0x70000000));
  EXPECT_TRUE(IsMarked(0xF0000000));

  EXPECT_FALSE(IsMarked(0x7FFFFFFF));
  EXPECT_TRUE(IsMarked(0xFFFFFFFF));

  EXPECT_FALSE(IsMarked(0x70000000));
  EXPECT_TRUE(IsMarked(0xC0000000));

  EXPECT_FALSE(IsMarked(0x0000BEEF));
  EXPECT_TRUE(IsMarked(0x8000BEEF));
}

TEST(ImageUtilsTest, MarkIndex) {
  EXPECT_EQ(offset_t(0x80000000), MarkIndex(0x00000000));
  EXPECT_EQ(offset_t(0x80000000), MarkIndex(0x80000000));

  EXPECT_EQ(offset_t(0x80000001), MarkIndex(0x00000001));
  EXPECT_EQ(offset_t(0x80000001), MarkIndex(0x80000001));

  EXPECT_EQ(offset_t(0xF0000000), MarkIndex(0x70000000));
  EXPECT_EQ(offset_t(0xF0000000), MarkIndex(0xF0000000));

  EXPECT_EQ(offset_t(0xFFFFFFFF), MarkIndex(0x7FFFFFFF));
  EXPECT_EQ(offset_t(0xFFFFFFFF), MarkIndex(0xFFFFFFFF));

  EXPECT_EQ(offset_t(0xC0000000), MarkIndex(0x40000000));
  EXPECT_EQ(offset_t(0xC0000000), MarkIndex(0xC0000000));

  EXPECT_EQ(offset_t(0x8000BEEF), MarkIndex(0x0000BEEF));
  EXPECT_EQ(offset_t(0x8000BEEF), MarkIndex(0x8000BEEF));
}

TEST(ImageUtilsTest, UnmarkIndex) {
  EXPECT_EQ(offset_t(0x00000000), UnmarkIndex(0x00000000));
  EXPECT_EQ(offset_t(0x00000000), UnmarkIndex(0x80000000));

  EXPECT_EQ(offset_t(0x00000001), UnmarkIndex(0x00000001));
  EXPECT_EQ(offset_t(0x00000001), UnmarkIndex(0x80000001));

  EXPECT_EQ(offset_t(0x70000000), UnmarkIndex(0x70000000));
  EXPECT_EQ(offset_t(0x70000000), UnmarkIndex(0xF0000000));

  EXPECT_EQ(offset_t(0x7FFFFFFF), UnmarkIndex(0x7FFFFFFF));
  EXPECT_EQ(offset_t(0x7FFFFFFF), UnmarkIndex(0xFFFFFFFF));

  EXPECT_EQ(offset_t(0x40000000), UnmarkIndex(0x40000000));
  EXPECT_EQ(offset_t(0x40000000), UnmarkIndex(0xC0000000));

  EXPECT_EQ(offset_t(0x0000BEEF), UnmarkIndex(0x0000BEEF));
  EXPECT_EQ(offset_t(0x0000BEEF), UnmarkIndex(0x8000BEEF));
}

}  // namespace zucchini
