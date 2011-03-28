// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/chevron_menu_button.h"
#import "chrome/browser/ui/cocoa/extensions/chevron_menu_button_cell.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

class ChevronMenuButtonTest : public CocoaTest {
 public:
  ChevronMenuButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    scoped_nsobject<ChevronMenuButton> button(
        [[ChevronMenuButton alloc] initWithFrame:frame]);
    button_ = button.get();
    [[test_window() contentView] addSubview:button_];
  }

  ChevronMenuButton* button_;
};

// Test basic view operation.
TEST_VIEW(ChevronMenuButtonTest, button_);

// |ChevronMenuButton exists entirely to override the cell class.
TEST_F(ChevronMenuButtonTest, CellSubclass) {
  EXPECT_TRUE([[button_ cell] isKindOfClass:[ChevronMenuButtonCell class]]);
}

// Test both hovered and non-hovered display.
TEST_F(ChevronMenuButtonTest, HoverAndNonHoverDisplay) {
  ChevronMenuButtonCell* cell = [button_ cell];
  EXPECT_FALSE([cell showsBorderOnlyWhileMouseInside]);
  EXPECT_FALSE([cell isMouseInside]);

  [cell setShowsBorderOnlyWhileMouseInside:YES];
  [cell mouseEntered:nil];
  EXPECT_TRUE([cell isMouseInside]);
  [button_ display];

  [cell mouseExited:nil];
  EXPECT_FALSE([cell isMouseInside]);
  [button_ display];
}

}  // namespace
