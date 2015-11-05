// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/move_loop.h"

#include "components/mus/public/cpp/tests/test_window.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

using MoveLoopTest = testing::Test;

namespace {

// Sets the client area for |window|. Padding is provided on the top so that
// there is a movable region. All other edges are sized to
// |MoveLoop::kResizeSize|.
void SetClientArea(mus::Window* window) {
  if (window->bounds().IsEmpty())
    window->SetBounds(gfx::Rect(100, 200, 300, 400));

  window->SetClientArea(
      gfx::Insets(MoveLoop::kResizeSize + 10, MoveLoop::kResizeSize,
                  MoveLoop::kResizeSize, MoveLoop::kResizeSize));
}

mus::mojom::EventPtr CreatePointerDownEvent(const gfx::Point& location) {
  const ui::TouchEvent event(ui::ET_TOUCH_PRESSED, location, 1,
                             base::TimeDelta());
  return mus::mojom::Event::From(static_cast<const ui::Event&>(event));
}

mus::mojom::EventPtr CreatePointerMove(const gfx::Point& location) {
  const ui::TouchEvent event(ui::ET_TOUCH_MOVED, location, 1,
                             base::TimeDelta());
  return mus::mojom::Event::From(static_cast<const ui::Event&>(event));
}

enum class HorizontalLocation {
  LEFT,
  MIDDLE,
  RIGHT,
};

enum class VerticalLocation {
  TOP_MOVE,
  TOP_RESIZE,
  MIDDLE,
  BOTTOM,
};

int HorizontalLocationToPoint(const mus::Window* window,
                              HorizontalLocation loc) {
  if (loc == HorizontalLocation::LEFT)
    return 0;
  return loc == HorizontalLocation::MIDDLE ? window->bounds().width() / 2
                                           : window->bounds().width() - 1;
}

int VerticalLocationToPoint(const mus::Window* window, VerticalLocation loc) {
  switch (loc) {
    case VerticalLocation::TOP_MOVE:
      return MoveLoop::kResizeSize + 1;
    case VerticalLocation::TOP_RESIZE:
      return 0;
    case VerticalLocation::MIDDLE:
      return window->bounds().height() / 2;
    case VerticalLocation::BOTTOM:
      return window->bounds().height() - 1;
  }
  return 0;
}

gfx::Point LocationToPoint(const mus::Window* window,
                           HorizontalLocation h_loc,
                           VerticalLocation v_loc) {
  return gfx::Point(HorizontalLocationToPoint(window, h_loc),
                    VerticalLocationToPoint(window, v_loc));
}

}  // namespace

TEST_F(MoveLoopTest, Move) {
  struct TestData {
    HorizontalLocation initial_h_loc;
    VerticalLocation initial_v_loc;
    // Number of values in delta_* that are valid. For example, if this is 1
    // then only one move is generated with thye point |delta_x[0]|,
    // |delta_y[0]|.
    int delta_count;
    int delta_x[3];
    int delta_y[3];
    gfx::Rect expected_bounds;
  } data[] = {
      // Move.
      {HorizontalLocation::MIDDLE,
       VerticalLocation::TOP_MOVE,
       1,
       {1, 0, 0},
       {2, 0, 0},
       gfx::Rect(101, 202, 300, 400)},

      // Resize from top, left.
      {HorizontalLocation::LEFT,
       VerticalLocation::TOP_RESIZE,
       1,
       {1, 0, 0},
       {2, 0, 0},
       gfx::Rect(101, 202, 299, 398)},

      // Resize from left.
      {HorizontalLocation::LEFT,
       VerticalLocation::MIDDLE,
       1,
       {1, 0, 0},
       {2, 0, 0},
       gfx::Rect(101, 200, 299, 400)},

      // Resize from bottom, left.
      {HorizontalLocation::LEFT,
       VerticalLocation::BOTTOM,
       1,
       {-1, 0, 0},
       {-2, 0, 0},
       gfx::Rect(99, 200, 301, 398)},

      // Resize from bottom.
      {HorizontalLocation::MIDDLE,
       VerticalLocation::BOTTOM,
       1,
       {-1, 0, 0},
       {4, 0, 0},
       gfx::Rect(100, 200, 300, 404)},

      // Resize from bottom, right.
      {HorizontalLocation::RIGHT,
       VerticalLocation::BOTTOM,
       1,
       {-3, 0, 0},
       {4, 0, 0},
       gfx::Rect(100, 200, 297, 404)},

      // Resize from right.
      {HorizontalLocation::RIGHT,
       VerticalLocation::MIDDLE,
       1,
       {6, 0, 0},
       {-4, 0, 0},
       gfx::Rect(100, 200, 306, 400)},

      // Resize from top, right.
      {HorizontalLocation::RIGHT,
       VerticalLocation::TOP_RESIZE,
       1,
       {6, 0, 0},
       {5, 0, 0},
       gfx::Rect(100, 205, 306, 395)},
  };

  for (size_t i = 0; i < arraysize(data); ++i) {
    mus::TestWindow window;
    SetClientArea(&window);
    gfx::Point pointer_location(
        LocationToPoint(&window, data[i].initial_h_loc, data[i].initial_v_loc));
    scoped_ptr<MoveLoop> move_loop =
        MoveLoop::Create(&window, *CreatePointerDownEvent(pointer_location));
    ASSERT_TRUE(move_loop.get()) << i;
    for (int j = 0; j < data[i].delta_count; ++j) {
      pointer_location.Offset(data[i].delta_x[j], data[i].delta_y[j]);
      ASSERT_EQ(MoveLoop::MoveResult::CONTINUE,
                move_loop->Move(*CreatePointerMove(pointer_location)))
          << i << " " << j;
    }
    ASSERT_EQ(data[i].expected_bounds, window.bounds());
  }
}
