// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/events/test/cocoa_test_event_utils.h"

// Fake BookmarkButton delegate to get a pong on mouse entered/exited
@interface FakeButtonDelegate : NSObject<BookmarkButtonDelegate> {
 @public
  int entered_;
  int exited_;
  BOOL canDragToTrash_;
  int didDragToTrashCount_;
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

- (BOOL)canDragBookmarkButtonToTrash:(BookmarkButton*)button {
  return canDragToTrash_;
}

- (void)didDragBookmarkToTrash:(BookmarkButton*)button {
  didDragToTrashCount_++;
}

- (void)bookmarkDragDidEnd:(BookmarkButton*)button
                 operation:(NSDragOperation)operation {
}
@end

namespace {

class BookmarkButtonTest : public CocoaProfileTest {
};

// Make sure nothing leaks
TEST_F(BookmarkButtonTest, Create) {
  base::scoped_nsobject<BookmarkButton> button;
  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);
}

// Test folder and empty node queries.
TEST_F(BookmarkButtonTest, FolderAndEmptyOrNot) {
  base::scoped_nsobject<BookmarkButton> button;
  base::scoped_nsobject<BookmarkButtonCell> cell;

  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);
  cell.reset([[BookmarkButtonCell alloc] initTextCell:@"hi mom"]);
  [button setCell:cell];

  EXPECT_TRUE([button isEmpty]);
  EXPECT_FALSE([button isFolder]);
  EXPECT_FALSE([button bookmarkNode]);

  NSEvent* downEvent =
      cocoa_test_event_utils::LeftMouseDownAtPoint(NSMakePoint(10,10));
  // Since this returns (does not actually begin a modal drag), success!
  [button beginDrag:downEvent];

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* node = model->bookmark_bar_node();
  [cell setBookmarkNode:node];
  EXPECT_FALSE([button isEmpty]);
  EXPECT_TRUE([button isFolder]);
  EXPECT_EQ([button bookmarkNode], node);

  node = model->AddURL(node, 0, base::ASCIIToUTF16("hi mom"),
                       GURL("http://www.google.com"));
  [cell setBookmarkNode:node];
  EXPECT_FALSE([button isEmpty]);
  EXPECT_FALSE([button isFolder]);
  EXPECT_EQ([button bookmarkNode], node);
}

TEST_F(BookmarkButtonTest, MouseEnterExitRedirect) {
  NSEvent* moveEvent =
      cocoa_test_event_utils::MouseEventAtPoint(NSMakePoint(10,10),
                                                NSMouseMoved,
                                                0);
  base::scoped_nsobject<BookmarkButton> button;
  base::scoped_nsobject<BookmarkButtonCell> cell;
  base::scoped_nsobject<FakeButtonDelegate> delegate(
      [[FakeButtonDelegate alloc] init]);
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

TEST_F(BookmarkButtonTest, DragToTrash) {
  base::scoped_nsobject<BookmarkButton> button;
  base::scoped_nsobject<BookmarkButtonCell> cell;
  base::scoped_nsobject<FakeButtonDelegate> delegate(
      [[FakeButtonDelegate alloc] init]);
  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0,0,500,500)]);
  cell.reset([[BookmarkButtonCell alloc] initTextCell:@"hi mom"]);
  [button setCell:cell];
  [button setDelegate:delegate];

  // Add a deletable bookmark to the button.
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* barNode = model->bookmark_bar_node();
  const BookmarkNode* node = model->AddURL(barNode, 0,
                                           base::ASCIIToUTF16("hi mom"),
                                           GURL("http://www.google.com"));
  [cell setBookmarkNode:node];

  // Verify that if canDragBookmarkButtonToTrash is NO then the button can't
  // be dragged to the trash.
  delegate.get()->canDragToTrash_ = NO;
  NSDragOperation operation = [button draggingSourceOperationMaskForLocal:NO];
  EXPECT_EQ(0u, operation & NSDragOperationDelete);
  operation = [button draggingSourceOperationMaskForLocal:YES];
  EXPECT_EQ(0u, operation & NSDragOperationDelete);

  // Verify that if canDragBookmarkButtonToTrash is YES then the button can
  // be dragged to the trash.
  delegate.get()->canDragToTrash_ = YES;
  operation = [button draggingSourceOperationMaskForLocal:NO];
  EXPECT_EQ(NSDragOperationDelete, operation & NSDragOperationDelete);
  operation = [button draggingSourceOperationMaskForLocal:YES];
  EXPECT_EQ(NSDragOperationDelete, operation & NSDragOperationDelete);

  // Verify that canDragBookmarkButtonToTrash is called when expected.
  delegate.get()->canDragToTrash_ = YES;
  EXPECT_EQ(0, delegate.get()->didDragToTrashCount_);
  [button draggedImage:nil endedAt:NSZeroPoint operation:NSDragOperationCopy];
  EXPECT_EQ(0, delegate.get()->didDragToTrashCount_);
  [button draggedImage:nil endedAt:NSZeroPoint operation:NSDragOperationMove];
  EXPECT_EQ(0, delegate.get()->didDragToTrashCount_);
  [button draggedImage:nil endedAt:NSZeroPoint operation:NSDragOperationDelete];
  EXPECT_EQ(1, delegate.get()->didDragToTrashCount_);
}

}
