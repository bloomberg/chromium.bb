// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_label_button.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class AvatarLabelButtonTest : public CocoaTest {
 public:
  AvatarLabelButtonTest() {
    NSRect content_frame = [[test_window() contentView] frame];
    button_.reset([[AvatarLabelButton alloc] initWithFrame:content_frame]);
    [[test_window() contentView] addSubview:button_];
  }

 protected:
  base::scoped_nsobject<AvatarLabelButton> button_;
};

TEST_VIEW(AvatarLabelButtonTest, button_)

TEST_F(AvatarLabelButtonTest, TestHighlight) {
  EXPECT_FALSE([[button_ cell] isHighlighted]);
  [button_ display];
  [[button_ cell] setHighlighted:YES];
  [button_ display];
}
