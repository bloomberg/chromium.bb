// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/close_button_texture.h"

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr_shell {

TEST(CloseButtonTextureTest, HitTest) {
  auto button = base::MakeUnique<CloseButtonTexture>();

  struct {
    gfx::PointF location;
    bool expected;
  } test_cases[] = {
      {gfx::PointF(0.f, 0.f), false},   {gfx::PointF(0.f, 1.0f), false},
      {gfx::PointF(1.0f, 1.0f), false}, {gfx::PointF(1.0f, 0.f), false},
      {gfx::PointF(0.5f, 0.5f), true},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected, button->HitTest(test_cases[i].location));
  }
}

}  // namespace vr_shell
