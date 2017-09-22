// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

#include "base/test/gtest_util.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PaintImageTest, Subsetting) {
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  EXPECT_EQ(image.width(), 100);
  EXPECT_EQ(image.height(), 100);

  PaintImage subset_rect_1 = image.MakeSubset(gfx::Rect(25, 25, 50, 50));
  EXPECT_EQ(subset_rect_1.width(), 50);
  EXPECT_EQ(subset_rect_1.height(), 50);
  EXPECT_EQ(subset_rect_1.subset_rect_, gfx::Rect(25, 25, 50, 50));

  PaintImage subset_rect_2 =
      subset_rect_1.MakeSubset(gfx::Rect(25, 25, 25, 25));
  EXPECT_EQ(subset_rect_2.width(), 25);
  EXPECT_EQ(subset_rect_2.height(), 25);
  EXPECT_EQ(subset_rect_2.subset_rect_, gfx::Rect(50, 50, 25, 25));

  EXPECT_EQ(image, image.MakeSubset(gfx::Rect(100, 100)));
  EXPECT_DCHECK_DEATH(subset_rect_2.MakeSubset(gfx::Rect(10, 10, 25, 25)));
  EXPECT_DCHECK_DEATH(image.MakeSubset(gfx::Rect()));
}

}  // namespace cc
