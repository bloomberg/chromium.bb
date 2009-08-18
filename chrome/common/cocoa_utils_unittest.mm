// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/cocoa_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CocoaUtilsTest, DispositionFromEventFlagsTest) {
  ASSERT_EQ(NEW_FOREGROUND_TAB,
            event_utils::DispositionFromEventFlags(NSCommandKeyMask));
  ASSERT_EQ(NEW_BACKGROUND_TAB,
            event_utils::DispositionFromEventFlags(NSCommandKeyMask |
                                                   NSShiftKeyMask));
  ASSERT_EQ(NEW_WINDOW,
            event_utils::DispositionFromEventFlags(NSShiftKeyMask));
  // The SAVE_TO_DISK disposition is not currently supported, so we use
  // CURRENT_TAB instead.
  ASSERT_EQ(CURRENT_TAB,
            event_utils::DispositionFromEventFlags(NSAlternateKeyMask));
  ASSERT_EQ(CURRENT_TAB, event_utils::DispositionFromEventFlags(0));
  ASSERT_EQ(CURRENT_TAB,
            event_utils::DispositionFromEventFlags(NSControlKeyMask));
}
