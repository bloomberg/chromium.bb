// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"

// Simple class to remember how many mouseEntered: and mouseExited:
// calls it gets.  Only used by BookmarkMouseForwarding but placed
// at the top of the file to keep it outside the anon namespace.
@interface ButtonRemembersMouseEnterExit : NSButton {
 @public
  int enters_;
  int exits_;
}
@end

@implementation ButtonRemembersMouseEnterExit
- (void)mouseEntered:(NSEvent*)event {
  enters_++;
}
- (void)mouseExited:(NSEvent*)event {
  exits_++;
}
@end


namespace {

class BookmarkButtonCellTest : public CocoaProfileTest {
};

// Make sure it's not totally bogus
TEST_F(BookmarkButtonCellTest, SizeForBounds) {
  NSRect frame = NSMakeRect(0, 0, 50, 30);
  base::scoped_nsobject<NSButton> view([[NSButton alloc] initWithFrame:frame]);
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  [view setCell:cell.get()];
  [[test_window() contentView] addSubview:view];

  NSRect r = NSMakeRect(0, 0, 100, 100);
  NSSize size = [cell.get() cellSizeForBounds:r];
  EXPECT_TRUE(size.width > 0 && size.height > 0);
  EXPECT_TRUE(size.width < 200 && size.height < 200);
}

// Make sure icon-only buttons are squeezed tightly.
TEST_F(BookmarkButtonCellTest, IconOnlySqueeze) {
  NSRect frame = NSMakeRect(0, 0, 50, 30);
  base::scoped_nsobject<NSButton> view([[NSButton alloc] initWithFrame:frame]);
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  [view setCell:cell.get()];
  [[test_window() contentView] addSubview:view];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  base::scoped_nsobject<NSImage> image(
      rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).CopyNSImage());
  EXPECT_TRUE(image.get());

  NSRect r = NSMakeRect(0, 0, 100, 100);
  [cell setBookmarkCellText:@"  " image:image];
  CGFloat two_space_width = [cell.get() cellSizeForBounds:r].width;
  [cell setBookmarkCellText:@" " image:image];
  CGFloat one_space_width = [cell.get() cellSizeForBounds:r].width;
  [cell setBookmarkCellText:@"" image:image];
  CGFloat zero_space_width = [cell.get() cellSizeForBounds:r].width;

  // Make sure the switch to "no title" is more significant than we
  // would otherwise see by decreasing the length of the title.
  CGFloat delta1 = two_space_width - one_space_width;
  CGFloat delta2 = one_space_width - zero_space_width;
  EXPECT_GT(delta2, delta1);

}

// Make sure the default from the base class is overridden.
TEST_F(BookmarkButtonCellTest, MouseEnterStuff) {
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  // Setting the menu should have no affect since we either share or
  // dynamically compose the menu given a node.
  [cell setMenu:[[[NSMenu alloc] initWithTitle:@"foo"] autorelease]];
  EXPECT_FALSE([cell menu]);

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* node = model->bookmark_bar_node();
  [cell setEmpty:NO];
  [cell setBookmarkNode:node];
  EXPECT_TRUE([cell showsBorderOnlyWhileMouseInside]);

  [cell setEmpty:YES];
  EXPECT_FALSE([cell.get() showsBorderOnlyWhileMouseInside]);
}

TEST_F(BookmarkButtonCellTest, BookmarkNode) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);

  const BookmarkNode* node = model->bookmark_bar_node();
  [cell setBookmarkNode:node];
  EXPECT_EQ(node, [cell bookmarkNode]);

  node = model->other_node();
  [cell setBookmarkNode:node];
  EXPECT_EQ(node, [cell bookmarkNode]);
}

TEST_F(BookmarkButtonCellTest, BookmarkMouseForwarding) {
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  base::scoped_nsobject<ButtonRemembersMouseEnterExit> button(
      [[ButtonRemembersMouseEnterExit alloc]
          initWithFrame:NSMakeRect(0, 0, 50, 50)]);
  [button setCell:cell.get()];
  EXPECT_EQ(0, button.get()->enters_);
  EXPECT_EQ(0, button.get()->exits_);
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:NSMakePoint(10,10)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0];
  [cell mouseEntered:event];
  EXPECT_TRUE(button.get()->enters_ && !button.get()->exits_);

  for (int i = 0; i < 3; i++)
    [cell mouseExited:event];
  EXPECT_EQ(button.get()->enters_, 1);
  EXPECT_EQ(button.get()->exits_, 3);
}

// Confirms a cell created in a nib is initialized properly
TEST_F(BookmarkButtonCellTest, Awake) {
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] init]);
  [cell awakeFromNib];
  EXPECT_EQ(NSLeftTextAlignment, [cell alignment]);
}

// Subfolder arrow details.
TEST_F(BookmarkButtonCellTest, FolderArrow) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* bar = model->bookmark_bar_node();
  const BookmarkNode* node = model->AddURL(bar, bar->child_count(),
                                           base::ASCIIToUTF16("title"),
                                           GURL("http://www.google.com"));
  base::scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initForNode:node
                                         text:@"small"
                                        image:nil
                               menuController:nil]);
  EXPECT_TRUE(cell.get());

  NSSize size = [cell cellSize];
  // sanity check
  EXPECT_GE(size.width, 2);
  EXPECT_GE(size.height, 2);

  // Once we turn on arrow drawing make sure there is now room for it.
  [cell setDrawFolderArrow:YES];
  NSSize arrowSize = [cell cellSize];
  EXPECT_GT(arrowSize.width, size.width);
}

TEST_F(BookmarkButtonCellTest, VerticalTextOffset) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* bar = model->bookmark_bar_node();
  const BookmarkNode* node = model->AddURL(bar, bar->child_count(),
                                           base::ASCIIToUTF16("title"),
                                           GURL("http://www.google.com"));

  base::scoped_nsobject<GradientButtonCell> gradient_cell(
      [[GradientButtonCell alloc] initTextCell:@"Testing"]);
  base::scoped_nsobject<BookmarkButtonCell> bookmark_cell(
      [[BookmarkButtonCell alloc] initForNode:node
                                         text:@"small"
                                        image:nil
                               menuController:nil]);

  ASSERT_TRUE(gradient_cell.get());
  ASSERT_TRUE(bookmark_cell.get());

  EXPECT_EQ(1, [gradient_cell verticalTextOffset]);
  EXPECT_EQ(0, [bookmark_cell verticalTextOffset]);

  EXPECT_NE([bookmark_cell verticalTextOffset],
            [gradient_cell verticalTextOffset]);
}

}  // namespace
