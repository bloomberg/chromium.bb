// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtk_util.h"

#include "base/stringprintf.h"
#include "ui/base/events.h"
#include "testing/gtest/include/gtest/gtest.h"

static const guint states[] = {
  GDK_LOCK_MASK,
  GDK_CONTROL_MASK,
  GDK_SHIFT_MASK,
  GDK_MOD1_MASK,
  GDK_BUTTON1_MASK,
  GDK_BUTTON2_MASK,
  GDK_BUTTON3_MASK
};

static const int flags[] = {
  ui::EF_CAPS_LOCK_DOWN,
  ui::EF_CONTROL_DOWN,
  ui::EF_SHIFT_DOWN,
  ui::EF_ALT_DOWN,
  ui::EF_LEFT_BUTTON_DOWN,
  ui::EF_MIDDLE_BUTTON_DOWN,
  ui::EF_RIGHT_BUTTON_DOWN
};

TEST(GTKUtilTest, TestEventFlagsFromGdkState) {
  ASSERT_EQ(arraysize(states), arraysize(flags));

  const int size = arraysize(states);
  for (int i = 0; i < size; ++i) {
    SCOPED_TRACE(base::StringPrintf(
                   "Checking EventFlagsFromGdkState: i = %d", i));
    EXPECT_EQ(flags[i], event_utils::EventFlagsFromGdkState(states[i]));
  }

  for (int i = 0; i < size; ++i) {
    for (int j = i + 1; j < size; ++j) {
      SCOPED_TRACE(base::StringPrintf(
                     "Checking EventFlagsFromGdkState: i = %d, j = %d", i, j));
      EXPECT_EQ(flags[i] | flags[j],
                event_utils::EventFlagsFromGdkState(states[i] | states[j]));
    }
  }
}
