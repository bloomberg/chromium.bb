// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"

namespace {

class HyperlinkTextViewTest : public CocoaTest {
 public:
  HyperlinkTextViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 50);
    scoped_nsobject<HyperlinkTextView> view(
        [[HyperlinkTextView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  HyperlinkTextView* view_;
};

TEST_VIEW(HyperlinkTextViewTest, view_);

TEST_F(HyperlinkTextViewTest, TestViewConfiguration) {
  EXPECT_FALSE([view_ isEditable]);
  EXPECT_FALSE([view_ drawsBackground]);
  EXPECT_FALSE([view_ isHorizontallyResizable]);
  EXPECT_FALSE([view_ isVerticallyResizable]);
}

}  // namespace
