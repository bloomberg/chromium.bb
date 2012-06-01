// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/slow_animation_event_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/event.h"

using aura::KeyEvent;

namespace ash {
namespace internal {

typedef testing::Test SlowAnimationEventFilterTest;

TEST_F(SlowAnimationEventFilterTest, Basics) {
  SlowAnimationEventFilter filter;

  // Raw shift key press.
  KeyEvent shift_pressed(ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, ui::EF_NONE);
  EXPECT_TRUE(filter.IsUnmodifiedShiftPressed(&shift_pressed));

  // Raw shift key release.
  KeyEvent shift_released(ui::ET_KEY_RELEASED,
                          ui::VKEY_SHIFT,
                          ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(filter.IsUnmodifiedShiftPressed(&shift_released));

  // Regular key press.
  KeyEvent a_pressed(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(filter.IsUnmodifiedShiftPressed(&a_pressed));

  // Shifted regular key press.
  KeyEvent shift_a_pressed(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(filter.IsUnmodifiedShiftPressed(&shift_a_pressed));

  // Control then shift.
  KeyEvent control_shift(ui::ET_KEY_PRESSED,
                         ui::VKEY_SHIFT,
                         ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(filter.IsUnmodifiedShiftPressed(&control_shift));

  // The key handler always returns false, because we want these keys to be
  // processed by the rest of the system.
  EXPECT_FALSE(filter.PreHandleKeyEvent(NULL, &shift_pressed));
  EXPECT_FALSE(filter.PreHandleKeyEvent(NULL, &shift_released));
  EXPECT_FALSE(filter.PreHandleKeyEvent(NULL, &a_pressed));
  EXPECT_FALSE(filter.PreHandleKeyEvent(NULL, &shift_a_pressed));
  EXPECT_FALSE(filter.PreHandleKeyEvent(NULL, &control_shift));
}

}  // namespace internal
}  // namespace ash
