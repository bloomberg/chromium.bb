// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/event_utils.h"

#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/events/event_constants.h"

namespace event_utils {

namespace {

const guint states[] = {
  GDK_LOCK_MASK,
  GDK_CONTROL_MASK,
  GDK_SHIFT_MASK,
  GDK_MOD1_MASK,
  GDK_BUTTON1_MASK,
  GDK_BUTTON2_MASK,
  GDK_BUTTON3_MASK
};

const int flags[] = {
  ui::EF_CAPS_LOCK_DOWN,
  ui::EF_CONTROL_DOWN,
  ui::EF_SHIFT_DOWN,
  ui::EF_ALT_DOWN,
  ui::EF_LEFT_MOUSE_BUTTON,
  ui::EF_MIDDLE_MOUSE_BUTTON,
  ui::EF_RIGHT_MOUSE_BUTTON
};

}  // namespace

TEST(EventUtilsTest, EventFlagsFromGdkState) {
  ASSERT_EQ(arraysize(states), arraysize(flags));

  const int size = arraysize(states);
  for (int i = 0; i < size; ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "Checking EventFlagsFromGdkState: i = %d", i));
    EXPECT_EQ(flags[i], EventFlagsFromGdkState(states[i]));
  }

  for (int i = 0; i < size; ++i) {
    for (int j = i + 1; j < size; ++j) {
      SCOPED_TRACE(base::StringPrintf(
          "Checking EventFlagsFromGdkState: i = %d, j = %d", i, j));
      EXPECT_EQ(flags[i] | flags[j],
                EventFlagsFromGdkState(states[i] | states[j]));
    }
  }
}

}  // namespace event_utils
