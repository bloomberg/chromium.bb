// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_view.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Fake NSDraggingInfo for testing
@interface FakeDraggingInfo : NSObject
@end

@implementation FakeDraggingInfo

- (id)draggingPasteboard {
  return self;
}

- (NSData*)dataForType:(NSString*)type {
  if ([type isEqual:kBookmarkButtonDragType])
    return [NSData dataWithBytes:&self length:sizeof(self)];
  return nil;
}

- (BOOL)draggingSource {
  return YES;  // pretend we're local
}

@end


// We are our own controller for test convenience.
@interface BookmarkBarFolderViewFakeController :
    BookmarkBarFolderView<BookmarkButtonControllerProtocol> {
 @public
  BOOL closedAll_;
  BOOL controllerEntered_;
  BOOL controllerExited_;
}
@end

@implementation BookmarkBarFolderViewFakeController

- (id<BookmarkButtonControllerProtocol>)controller {
  return self;
}

- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)pt {
  return YES;
}

- (CGFloat)indicatorPosForDragOfButton:(BookmarkButton*)button
                               toPoint:(NSPoint)point {
  return 101.0;  // Arbitrary value.
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  controllerEntered_ = YES;
  return NSDragOperationMove;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
  controllerExited_ = YES;
}

- (BOOL)dragShouldLockBarVisibility {
  return NO;
}

- (void)closeAllBookmarkFolders {
  closedAll_ = YES;
}

- (BOOL)dragButton:(BookmarkButton*)sourceButton
                to:(NSPoint)point
              copy:(BOOL)copy {
  return NO;
}

- (BookmarkModel*)bookmarkModel {
  NOTREACHED();
  return NULL;
}

- (void)closeBookmarkFolder:(id)sender {
}

- (NSWindow*)parentWindow {
  return nil;
}

- (ThemeProvider*)themeProvider {
  return nil;
}

- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child {
}

- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child {
}

- (void)openBookmarkNodesRecursive:(const BookmarkNode*)node
                       disposition:(WindowOpenDisposition)disposition {
}

- (void)addNewFolderControllerWithParentButton:(BookmarkButton*)parentButton {
}

- (BookmarkBarFolderController*)folderController {
  return nil;
}

@end

namespace {

class BookmarkBarFolderViewTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    view_.reset([[BookmarkBarFolderViewFakeController alloc] init]);
  }

  scoped_nsobject<BookmarkBarFolderViewFakeController> view_;
};

TEST_F(BookmarkBarFolderViewTest, Basics) {
  [view_ awakeFromNib];
  [[test_window() contentView] addSubview:view_];

  // Confirm an assumption made in our awakeFromNib
  EXPECT_FALSE([view_ showsDivider]);

  // Make sure we're set up for DnD
  NSArray* types = [view_ registeredDraggedTypes];
  EXPECT_TRUE([types containsObject:kBookmarkButtonDragType]);

  // This doesn't confirm results but it makes sure we don't crash or leak.
  [view_ drawRect:NSMakeRect(0,0,10,10)];
  [view_ setDropIndicatorShown:YES];
  [view_ drawRect:NSMakeRect(0,0,10,10)];

  [view_ removeFromSuperview];
}

TEST_F(BookmarkBarFolderViewTest, SimpleDragEnterExit) {
  [view_ awakeFromNib];
  scoped_nsobject<FakeDraggingInfo> info([[FakeDraggingInfo alloc] init]);

  [view_ draggingEntered:(id<NSDraggingInfo>)info.get()];
  // Confirms we got a chance to hover-open.
  EXPECT_TRUE(view_.get()->controllerEntered_);

  [view_ draggingEnded:(id<NSDraggingInfo>)info.get()];
  EXPECT_TRUE(view_.get()->controllerExited_);
}

}  // namespace
