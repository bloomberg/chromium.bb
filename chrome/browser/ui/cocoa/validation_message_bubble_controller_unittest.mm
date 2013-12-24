// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/validation_message_bubble_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static NSView* constructView(const char* main_text, const char* sub_text) {
  return [ValidationMessageBubbleController
      constructContentView:base::UTF8ToUTF16(main_text)
                   subText:base::UTF8ToUTF16(sub_text)];
}

TEST(ValidationMessageBubbleControllerTest, FrameSize) {
  NSRect shortMainNoSubFrame = [constructView("abc", "") frame];
  EXPECT_GE(NSWidth(shortMainNoSubFrame), 40);
  EXPECT_GT(NSHeight(shortMainNoSubFrame), 0);

  NSRect longMainNoSubFrame = [constructView(
      "very very very long text which overlfows the maximum window width. "
      "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod"
      " tempor incididunt ut labore et dolore magna aliqua.", "") frame];
  EXPECT_LT(NSWidth(longMainNoSubFrame), 500);
  EXPECT_LT(NSHeight(shortMainNoSubFrame), NSHeight(longMainNoSubFrame));

  NSRect shortMainMediumSubFrame = [constructView("abc", "foo bar baz") frame];
  EXPECT_GT(NSWidth(shortMainMediumSubFrame), NSWidth(shortMainNoSubFrame));
  EXPECT_GT(NSHeight(shortMainMediumSubFrame), NSHeight(shortMainNoSubFrame));

  NSRect shortMainLongSubFrame =  [constructView("abc",
      "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod"
      " tempor incididunt ut labore et dolore magna aliqua.") frame];
  EXPECT_GT(NSWidth(shortMainLongSubFrame), NSWidth(shortMainMediumSubFrame));
  EXPECT_LT(NSWidth(shortMainLongSubFrame), 500);
  EXPECT_GT(NSHeight(shortMainLongSubFrame), NSHeight(shortMainMediumSubFrame));
}

}

