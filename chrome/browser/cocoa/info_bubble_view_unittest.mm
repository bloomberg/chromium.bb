// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/info_bubble_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"

namespace {

class InfoBubbleViewTest : public CocoaTest {
 public:
  InfoBubbleViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    scoped_nsobject<InfoBubbleView> view(
        [[InfoBubbleView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  InfoBubbleView* view_;
};

TEST_VIEW(InfoBubbleViewTest, view_);

}  // namespace
