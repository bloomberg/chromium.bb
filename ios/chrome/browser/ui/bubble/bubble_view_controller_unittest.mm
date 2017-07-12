// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fixture to test BubbleViewController.
class BubbleViewControllerTest : public PlatformTest {};

// Tests the bubble's intrinsic content size given short text.
TEST_F(BubbleViewControllerTest, BubbleSizeShortText) {}

// Tests that the bubble accommodates text that exceeds the maximum bubble width
// by displaying the text on multiple lines.
TEST_F(BubbleViewControllerTest, BubbleSizeMultipleLineText) {}

// Tests that the bubble attaches to the UI element when the layout is flipped
// for RTL languages.
TEST_F(BubbleViewControllerTest, RTL) {}

// Tests that the accessibility label matches the display text.
TEST_F(BubbleViewControllerTest, Accessibility) {}
