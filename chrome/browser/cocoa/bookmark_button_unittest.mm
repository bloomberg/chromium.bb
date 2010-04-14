// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/test_event_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Fake BookmarkButton delegate to get a pong on mouse entered/exited
@interface FakeButtonDelegate : NSObject<BookmarkButtonDelegate> {
 @public
  int entered_;
  int exited_;
}
@end

@implementation FakeButtonDelegate

- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button {
}

- (void)mouseEnteredButton:(id)buton event:(NSEvent*)event {
  entered_++;
}

- (void)mouseExitedButton:(id)buton event:(NSEvent*)event {
  exited_++;
}

- (BOOL)dragShouldLockBarVisibility {
  return NO;
}

- (NSWindow*)browserWindow {
  return nil;
}
@end

namespace {

class BookmarkButtonTest : public CocoaTest {
};

// Make sure nothing leaks
TEST_F(BookmarkButtonTest, Create) {
  scoped_nsobject<BookmarkButton> button;
  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);
}

// Test folder and empty node queries.
TEST_F(BookmarkButtonTest, FolderAndEmptyOrNot) {
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkButton> button;
  scoped_nsobject<BookmarkButtonCell> cell;

  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);
  cell.reset([[BookmarkButtonCell alloc] initTextCell:@"hi mom"]);
  [button setCell:cell];

  EXPECT_TRUE([button isEmpty]);
  EXPECT_FALSE([button isFolder]);
  EXPECT_FALSE([button bookmarkNode]);

  NSEvent* downEvent =
      test_event_utils::LeftMouseDownAtPoint(NSMakePoint(10,10));
  // Since this returns (does not actually begin a modal drag), success!
  [button beginDrag:downEvent];

  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [cell setBookmarkNode:node];
  EXPECT_FALSE([button isEmpty]);
  EXPECT_TRUE([button isFolder]);
  EXPECT_EQ([button bookmarkNode], node);

  node = model->AddURL(node, 0, L"hi mom", GURL("http://www.google.com"));
  [cell setBookmarkNode:node];
  EXPECT_FALSE([button isEmpty]);
  EXPECT_FALSE([button isFolder]);
  EXPECT_EQ([button bookmarkNode], node);
}

TEST_F(BookmarkButtonTest, MouseEnterExitRedirect) {
  NSEvent* moveEvent =
      test_event_utils::MouseEventAtPoint(NSMakePoint(10,10), NSMouseMoved, 0);
  scoped_nsobject<BookmarkButton> button;
  scoped_nsobject<BookmarkButtonCell> cell;
  scoped_nsobject<FakeButtonDelegate>
      delegate([[FakeButtonDelegate alloc] init]);
  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);
  cell.reset([[BookmarkButtonCell alloc] initTextCell:@"hi mom"]);
  [button setCell:cell];
  [button setDelegate:delegate];

  EXPECT_EQ(0, delegate.get()->entered_);
  EXPECT_EQ(0, delegate.get()->exited_);

  [button mouseEntered:moveEvent];
  EXPECT_EQ(1, delegate.get()->entered_);
  EXPECT_EQ(0, delegate.get()->exited_);

  [button mouseExited:moveEvent];
  [button mouseExited:moveEvent];
  EXPECT_EQ(1, delegate.get()->entered_);
  EXPECT_EQ(2, delegate.get()->exited_);
}

}
