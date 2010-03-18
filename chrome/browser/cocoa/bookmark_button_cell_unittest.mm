// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_menu.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

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

// Make sure the default from the base class is overridden.
TEST_F(BookmarkButtonCellTest, MouseEnterStuff) {
  scoped_nsobject<BookmarkButtonCell> cell(
      [[BookmarkButtonCell alloc] initTextCell:@"Testing"]);
  [cell setMenu:[[[BookmarkMenu alloc] initWithTitle:@"foo"] autorelease]];

  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [cell setBookmarkNode:node];

  EXPECT_TRUE([cell.get() showsBorderOnlyWhileMouseInside]);
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

}  // namespace
