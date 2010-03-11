// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {
const CGFloat kFakeIndicatorPos = 7.0;
};

// Fake DraggingInfo, fake BookmarkBarController, fake pasteboard...
@interface FakeBookmarkDraggingInfo : NSObject {
 @public
  scoped_nsobject<NSData> data_;
  BOOL pong_;
  BOOL dropIndicatorShown_;
  BOOL draggingEnteredCalled_;
}
@property (readwrite) BOOL dropIndicatorShown;
@property (readwrite) BOOL draggingEnteredCalled;
@end

@implementation FakeBookmarkDraggingInfo

@synthesize dropIndicatorShown = dropIndicatorShown_;
@synthesize draggingEnteredCalled = draggingEnteredCalled_;

- (id)init {
  if ((self = [super init])) {
    dropIndicatorShown_ = YES;
    draggingEnteredCalled_ = NO;
    data_.reset([[NSData dataWithBytes:&self length:sizeof(self)] retain]);
  }
  return self;
}

// So we can be both info and pasteboard.
- (id)draggingPasteboard {
  return self;
}

// So we can look local.
- (id)draggingSource {
  return self;
}

- (BOOL)containsURLData {
  return NO;
}

- (NSPoint)draggingLocation {
  return NSMakePoint(10, 10);
}

- (NSData*)dataForType:(NSString*)type {
  if ([type isEqual:kBookmarkButtonDragType])
    return data_.get();
  return nil;
}

// Fake a controller for callback ponging
- (BOOL)dragButton:(BookmarkButton*)button to:(NSPoint)point copy:(BOOL)copy {
  pong_ = YES;
  return YES;
}

// Confirm the pong.
- (BOOL)dragButtonToPong {
  return pong_;
}

- (CGFloat)indicatorPosForDragOfButton:(BookmarkButton*)sourceButton
                               toPoint:(NSPoint)point {
  return kFakeIndicatorPos;
}

- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)point {
  return dropIndicatorShown_;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  draggingEnteredCalled_ = YES;
  return NSDragOperationNone;
}

- (NSDragOperation)draggingSourceOperationMask {
  return NSDragOperationCopy | NSDragOperationMove;
}

@end

namespace {

class BookmarkBarViewTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    view_.reset([[BookmarkBarView alloc] init]);
  }

  scoped_nsobject<BookmarkBarView> view_;
};

TEST_F(BookmarkBarViewTest, CanDragWindow) {
  EXPECT_FALSE([view_ mouseDownCanMoveWindow]);
}

TEST_F(BookmarkBarViewTest, BookmarkButtonDragAndDrop) {
  scoped_nsobject<FakeBookmarkDraggingInfo>
      info([[FakeBookmarkDraggingInfo alloc] init]);

  [view_ setController:info.get()];
  EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
  EXPECT_TRUE([view_ performDragOperation:(id)info.get()]);
  EXPECT_TRUE([info dragButtonToPong]);
}

TEST_F(BookmarkBarViewTest, BookmarkButtonDropIndicator) {
  scoped_nsobject<FakeBookmarkDraggingInfo>
      info([[FakeBookmarkDraggingInfo alloc] init]);

  [view_ setController:info.get()];
  EXPECT_FALSE([info draggingEnteredCalled]);
  EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
  EXPECT_TRUE([info draggingEnteredCalled]);  // Ensure controller pingged.
  EXPECT_TRUE([view_ dropIndicatorShown]);
  EXPECT_EQ([view_ dropIndicatorPosition], kFakeIndicatorPos);

  [info setDropIndicatorShown:NO];
  EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
  EXPECT_FALSE([view_ dropIndicatorShown]);
}

}  // namespace
