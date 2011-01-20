// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "grit/app_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"

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

class BookmarkButtonCellTest : public CocoaTest {
  public:
    BrowserTestHelper helper_;
};

// Make sure it's not totally bogus
TEST_F(BookmarkButtonCellTest, SizeForBounds) {
  NSRect frame = NSMakeRect(0, 0, 50, 30);
  scoped_nsobject<NSButton> view([[NSButton alloc] initWithFrame:frame]);
  scoped_nsobject<BookmarkButtonCell> cell(
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
  scoped_nsobject<NSButton> view([[NSButton alloc] initWithFrame:frame]);
  scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  [view setCell:cell.get()];
  [[test_window() contentView] addSubview:view];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_nsobject<NSImage> image([rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON)
                                    retain]);
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
  scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  // Setting the menu should have no affect since we either share or
  // dynamically compose the menu given a node.
  [cell setMenu:[[[BookmarkMenu alloc] initWithTitle:@"foo"] autorelease]];
  EXPECT_FALSE([cell menu]);

  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [cell setEmpty:NO];
  [cell setBookmarkNode:node];
  EXPECT_TRUE([cell showsBorderOnlyWhileMouseInside]);
  EXPECT_TRUE([cell menu]);

  [cell setEmpty:YES];
  EXPECT_FALSE([cell.get() showsBorderOnlyWhileMouseInside]);
  EXPECT_FALSE([cell menu]);
}

TEST_F(BookmarkButtonCellTest, BookmarkNode) {
  BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
  scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);

  const BookmarkNode* node = model.GetBookmarkBarNode();
  [cell setBookmarkNode:node];
  EXPECT_EQ(node, [cell bookmarkNode]);

  node = model.other_node();
  [cell setBookmarkNode:node];
  EXPECT_EQ(node, [cell bookmarkNode]);
}

TEST_F(BookmarkButtonCellTest, BookmarkMouseForwarding) {
  scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  scoped_nsobject<ButtonRemembersMouseEnterExit>
    button([[ButtonRemembersMouseEnterExit alloc]
             initWithFrame:NSMakeRect(0,0,50,50)]);
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
  scoped_nsobject<BookmarkButtonCell> cell([[BookmarkButtonCell alloc] init]);
  [cell awakeFromNib];
  EXPECT_EQ(NSLeftTextAlignment, [cell alignment]);
}

// Subfolder arrow details.
TEST_F(BookmarkButtonCellTest, FolderArrow) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* bar = model->GetBookmarkBarNode();
  const BookmarkNode* node = model->AddURL(bar, bar->GetChildCount(),
                                           ASCIIToUTF16("title"),
                                           GURL("http://www.google.com"));
  scoped_nsobject<BookmarkButtonCell> cell(
    [[BookmarkButtonCell alloc] initForNode:node
                                contextMenu:nil
                                   cellText:@"small"
                                  cellImage:nil]);
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

}  // namespace
