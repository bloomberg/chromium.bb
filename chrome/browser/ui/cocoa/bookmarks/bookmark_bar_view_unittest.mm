// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"

namespace {
  const CGFloat kFakeIndicatorPos = 7.0;
};

// Fake DraggingInfo, fake BookmarkBarController, fake NSPasteboard...
@interface FakeBookmarkDraggingInfo : NSObject {
 @public
  BOOL dragButtonToPong_;
  BOOL dragURLsPong_;
  BOOL dragBookmarkDataPong_;
  BOOL dropIndicatorShown_;
  BOOL draggingEnteredCalled_;
  // Only mock one type of drag data at a time.
  NSString* dragDataType_;
}
@property (nonatomic) BOOL dropIndicatorShown;
@property (nonatomic) BOOL draggingEnteredCalled;
@property (nonatomic, copy) NSString* dragDataType;
@end

@implementation FakeBookmarkDraggingInfo

@synthesize dropIndicatorShown = dropIndicatorShown_;
@synthesize draggingEnteredCalled = draggingEnteredCalled_;
@synthesize dragDataType = dragDataType_;

- (id)init {
  if ((self = [super init])) {
    dropIndicatorShown_ = YES;
  }
  return self;
}

- (void)dealloc {
  [dragDataType_ release];
  [super dealloc];
}

- (void)reset {
  [dragDataType_ release];
  dragDataType_ = nil;
  dragButtonToPong_ = NO;
  dragURLsPong_ = NO;
  dragBookmarkDataPong_ = NO;
  dropIndicatorShown_ = YES;
  draggingEnteredCalled_ = NO;
}

// NSDragInfo mocking functions.

- (id)draggingPasteboard {
  return self;
}

// So we can look local.
- (id)draggingSource {
  return self;
}

- (NSDragOperation)draggingSourceOperationMask {
  return NSDragOperationCopy | NSDragOperationMove;
}

- (NSPoint)draggingLocation {
  return NSMakePoint(10, 10);
}

// NSPasteboard mocking functions.

- (BOOL)containsURLData {
  NSArray* urlTypes = [URLDropTargetHandler handledDragTypes];
  if (dragDataType_)
    return [urlTypes containsObject:dragDataType_];
  return NO;
}

- (NSData*)dataForType:(NSString*)type {
  if (dragDataType_ && [dragDataType_ isEqualToString:type])
    return [NSData data];  // Return something, anything.
  return nil;
}

// Fake a controller for callback ponging

- (BOOL)dragButton:(BookmarkButton*)button to:(NSPoint)point copy:(BOOL)copy {
  dragButtonToPong_ = YES;
  return YES;
}

- (BOOL)addURLs:(NSArray*)urls withTitles:(NSArray*)titles at:(NSPoint)point {
  dragURLsPong_ = YES;
  return YES;
}

- (void)getURLs:(NSArray**)outUrls
    andTitles:(NSArray**)outTitles
    convertingFilenames:(BOOL)convertFilenames {
}

- (BOOL)dragBookmarkData:(id<NSDraggingInfo>)info {
  dragBookmarkDataPong_ = YES;
  return NO;
}

- (BOOL)canEditBookmarks {
  return YES;
}

// Confirm the pongs.

- (BOOL)dragButtonToPong {
  return dragButtonToPong_;
}

- (BOOL)dragURLsPong {
  return dragURLsPong_;
}

- (BOOL)dragBookmarkDataPong {
  return dragBookmarkDataPong_;
}

- (CGFloat)indicatorPosForDragToPoint:(NSPoint)point {
  return kFakeIndicatorPos;
}

- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)point {
  return dropIndicatorShown_;
}

- (BOOL)draggingAllowed:(id<NSDraggingInfo>)info {
  return YES;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  draggingEnteredCalled_ = YES;
  return NSDragOperationNone;
}

- (void)setDropInsertionPos:(CGFloat)where {
}

- (void)clearDropInsertionPos {
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
  [info reset];

  [info setDragDataType:kBookmarkButtonDragType];
  EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
  EXPECT_TRUE([view_ performDragOperation:(id)info.get()]);
  EXPECT_TRUE([info dragButtonToPong]);
  EXPECT_FALSE([info dragURLsPong]);
  EXPECT_TRUE([info dragBookmarkDataPong]);
}

TEST_F(BookmarkBarViewTest, URLDragAndDrop) {
  scoped_nsobject<FakeBookmarkDraggingInfo>
      info([[FakeBookmarkDraggingInfo alloc] init]);
  [view_ setController:info.get()];
  [info reset];

  NSArray* dragTypes = [URLDropTargetHandler handledDragTypes];
  for (NSString* type in dragTypes) {
    [info setDragDataType:type];
    EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
    EXPECT_TRUE([view_ performDragOperation:(id)info.get()]);
    EXPECT_FALSE([info dragButtonToPong]);
    EXPECT_TRUE([info dragURLsPong]);
    EXPECT_TRUE([info dragBookmarkDataPong]);
    [info reset];
  }
}

TEST_F(BookmarkBarViewTest, BookmarkButtonDropIndicator) {
  scoped_nsobject<FakeBookmarkDraggingInfo>
      info([[FakeBookmarkDraggingInfo alloc] init]);
  [view_ setController:info.get()];

  [info reset];
  [info setDragDataType:kBookmarkButtonDragType];
  EXPECT_FALSE([info draggingEnteredCalled]);
  EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
  EXPECT_TRUE([info draggingEnteredCalled]);  // Ensure controller pinged.
  EXPECT_TRUE([view_ dropIndicatorShown]);
  EXPECT_EQ([view_ dropIndicatorPosition], kFakeIndicatorPos);

  [info setDropIndicatorShown:NO];
  EXPECT_EQ([view_ draggingEntered:(id)info.get()], NSDragOperationMove);
  EXPECT_FALSE([view_ dropIndicatorShown]);
}

}  // namespace
