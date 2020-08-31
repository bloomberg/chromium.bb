// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/keyboard_event_counter.h"

#include <memory>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace media {

TEST(KeyboardEventCounterTest, KeyPressCounter) {
  KeyboardEventCounter counter;

  EXPECT_EQ(0u, counter.GetKeyPressCount());

  counter.OnKeyboardEvent(ui::ET_KEY_PRESSED, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  // Holding the same key without releasing it does not increase the count.
  counter.OnKeyboardEvent(ui::ET_KEY_PRESSED, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  // Releasing the key does not affect the total count.
  counter.OnKeyboardEvent(ui::ET_KEY_RELEASED, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  counter.OnKeyboardEvent(ui::ET_KEY_PRESSED, ui::VKEY_0);
  counter.OnKeyboardEvent(ui::ET_KEY_RELEASED, ui::VKEY_0);
  EXPECT_EQ(2u, counter.GetKeyPressCount());
}

}  // namespace media
