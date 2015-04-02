// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/server_view.h"
#include "mojo/services/view_manager/server_view_delegate.h"
#include "mojo/services/view_manager/test_server_view_delegate.h"
#include "mojo/services/view_manager/view_coordinate_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace view_manager {

using ViewCoordinateConversionsTest = testing::Test;

TEST_F(ViewCoordinateConversionsTest, ConvertRectBetweenViews) {
  TestServerViewDelegate d1, d2, d3;
  ServerView v1(&d1, ViewId()), v2(&d2, ViewId()), v3(&d3, ViewId());
  v1.SetBounds(gfx::Rect(1, 2, 100, 100));
  v2.SetBounds(gfx::Rect(3, 4, 100, 100));
  v3.SetBounds(gfx::Rect(5, 6, 100, 100));
  v1.Add(&v2);
  v2.Add(&v3);

  EXPECT_EQ(gfx::Rect(2, 1, 8, 9),
            ConvertRectBetweenViews(&v1, &v3, gfx::Rect(10, 11, 8, 9)));

  EXPECT_EQ(gfx::Rect(18, 21, 8, 9),
            ConvertRectBetweenViews(&v3, &v1, gfx::Rect(10, 11, 8, 9)));
}

}  // namespace view_manager
