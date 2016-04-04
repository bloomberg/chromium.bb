// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/frame/move_loop.h"

#include <stddef.h>

#include "base/macros.h"
#include "components/mus/public/cpp/tests/test_window.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

using MoveLoopTest = testing::Test;

namespace mash {
namespace wm {

TEST_F(MoveLoopTest, Move) {
  struct TestData {
    // One of the HitTestCompat values.
    int ht_location;
    // Amount to move the mouse by.
    int delta_x;
    int delta_y;
    gfx::Rect expected_bounds;
  } data[] = {
      // Move.
      {HTCAPTION, 1, 2, gfx::Rect(101, 202, 300, 400)},

      // Resizing
      {HTTOPLEFT, 1, 2, gfx::Rect(101, 202, 299, 398)},
      {HTLEFT, 1, 2, gfx::Rect(101, 200, 299, 400)},
      {HTBOTTOMLEFT, -1, -2, gfx::Rect(99, 200, 301, 398)},
      {HTBOTTOM, -1, 4, gfx::Rect(100, 200, 300, 404)},
      {HTBOTTOMRIGHT, -3, 4, gfx::Rect(100, 200, 297, 404)},
      {HTRIGHT, 6, -4, gfx::Rect(100, 200, 306, 400)},
      {HTTOPRIGHT, 6, 5, gfx::Rect(100, 205, 306, 395)},
  };

  for (size_t i = 0; i < arraysize(data); ++i) {
    mus::TestWindow window;
    window.SetBounds(gfx::Rect(100, 200, 300, 400));
    gfx::Point pointer_location(51, 52);
    ui::PointerEvent pointer_down_event(
        ui::ET_POINTER_DOWN, ui::EventPointerType::POINTER_TYPE_TOUCH,
        pointer_location, pointer_location, ui::EF_NONE, 1, base::TimeDelta());
    std::unique_ptr<MoveLoop> move_loop =
        MoveLoop::Create(&window, data[i].ht_location, pointer_down_event);
    ASSERT_TRUE(move_loop.get()) << i;
    pointer_location.Offset(data[i].delta_x, data[i].delta_y);
    ui::PointerEvent pointer_move_event(
        ui::ET_POINTER_MOVED, ui::EventPointerType::POINTER_TYPE_TOUCH,
        pointer_location, pointer_location, ui::EF_NONE, 1, base::TimeDelta());
    ASSERT_EQ(MoveLoop::MoveResult::CONTINUE,
              move_loop->Move(pointer_move_event))
        << i;
    ASSERT_EQ(data[i].expected_bounds, window.bounds());
  }
}

}  // namespace wm
}  // namespace mash
