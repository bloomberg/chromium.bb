// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/down_arrow_popup_menu_cell.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class DownArrowPopupMenuCellTest : public ui::CocoaTest {
 public:
  DownArrowPopupMenuCellTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    view_.reset([[NSButton alloc] initWithFrame:frame]);
    base::scoped_nsobject<DownArrowPopupMenuCell> cell(
        [[DownArrowPopupMenuCell alloc] initTextCell:@"Testing"]);
    [view_ setCell:cell.get()];
    [[test_window() contentView] addSubview:view_];  }

 protected:
  base::scoped_nsobject<NSButton> view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownArrowPopupMenuCellTest);
};

TEST_VIEW(DownArrowPopupMenuCellTest, view_)

}  // namespace

// Make internal messages visible for testing
@interface DownArrowPopupMenuCell (Testing)

- (NSSize)cellSize;
- (NSSize)imageSize;

@end

// Test size computations, make sure they deliver correct results.
TEST_F(DownArrowPopupMenuCellTest, Defaults) {
  DownArrowPopupMenuCell* cell =
      static_cast<DownArrowPopupMenuCell*>([view_ cell]);
  ASSERT_TRUE([cell isKindOfClass:[DownArrowPopupMenuCell class]]);

  NSRect rect = NSMakeRect(0, 0, 11, 17);
  base::scoped_nsobject<NSImage> image(
      [[NSImage alloc] initWithSize:rect.size]);
  [cell setImage:image forButtonState:image_button_cell::kDefaultState];
  [view_ setTitle:@"Testing"];

  EXPECT_EQ(NSWidth(rect), [cell imageSize].width);
  EXPECT_EQ(NSHeight(rect), [cell imageSize].height);

  NSAttributedString* title = [cell attributedTitle];
  NSSize titleSize = [title size];
  EXPECT_LE(titleSize.width + [image size].width + autofill::kButtonGap,
            [cell cellSize].width);
  EXPECT_LE(std::max(titleSize.height, [image size].height),
            [cell cellSize].height);
}
