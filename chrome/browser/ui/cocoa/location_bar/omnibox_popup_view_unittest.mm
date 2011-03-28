// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/location_bar/omnibox_popup_view.h"

namespace {

class OmniboxPopupViewTest : public CocoaTest {
 public:
  OmniboxPopupViewTest() {
    NSRect content_frame = [[test_window() contentView] frame];
    scoped_nsobject<OmniboxPopupView> view(
        [[OmniboxPopupView alloc] initWithFrame:content_frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  OmniboxPopupView* view_;  // Weak. Owned by the view hierarchy.
};

// Tests display, add/remove.
TEST_VIEW(OmniboxPopupViewTest, view_);

// A single subview should completely fill the popup view.
TEST_F(OmniboxPopupViewTest, ResizeWithOneSubview) {
  scoped_nsobject<NSView> subview1([[NSView alloc] initWithFrame:NSZeroRect]);

  // Adding the subview should not change its frame.
  [view_ addSubview:subview1];
  EXPECT_TRUE(NSEqualRects(NSZeroRect, [subview1 frame]));

  // Resizing the popup view should also resize the subview.
  [view_ setFrame:NSMakeRect(0, 0, 100, 100)];
  EXPECT_TRUE(NSEqualRects([view_ bounds], [subview1 frame]));
}

TEST_F(OmniboxPopupViewTest, ResizeWithTwoSubviews) {
  const CGFloat height = 50;
  NSRect initial = NSMakeRect(0, 0, 100, height);

  scoped_nsobject<NSView> subview1([[NSView alloc] initWithFrame:NSZeroRect]);
  scoped_nsobject<NSView> subview2([[NSView alloc] initWithFrame:initial]);
  [view_ addSubview:subview1];
  [view_ addSubview:subview2];

  // Resize the popup view to be much larger than height.  |subview2|'s height
  // should stay the same, and |subview1| should resize to fill all available
  // space.
  [view_ setFrame:NSMakeRect(0, 0, 300, 4 * height)];
  EXPECT_EQ(NSWidth([view_ frame]), NSWidth([subview1 frame]));
  EXPECT_EQ(NSWidth([view_ frame]), NSWidth([subview2 frame]));
  EXPECT_EQ(height, NSHeight([subview2 frame]));
  EXPECT_EQ(NSHeight([view_ frame]),
            NSHeight([subview1 frame]) + NSHeight([subview2 frame]));

  // Now resize the popup view to be smaller than height.  |subview2|'s height
  // should stay the same, and |subview1|'s height should be zero, not negative.
  [view_ setFrame:NSMakeRect(0, 0, 300, height - 10)];
  EXPECT_EQ(NSWidth([view_ frame]), NSWidth([subview1 frame]));
  EXPECT_EQ(NSWidth([view_ frame]), NSWidth([subview2 frame]));
  EXPECT_EQ(0, NSHeight([subview1 frame]));
  EXPECT_EQ(height, NSHeight([subview2 frame]));
}

}  // namespace
