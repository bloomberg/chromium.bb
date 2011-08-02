// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"

// Allows us to verify BookmarkBarFolderView.
@interface BookmarkBarFolderView(TestingAPI)

@property(readonly, nonatomic) BOOL dropIndicatorShown;
@property(readonly, nonatomic) CGFloat dropIndicatorPosition;

-(void)setController:(id<BookmarkButtonControllerProtocol>)controller;

@end

@implementation BookmarkBarFolderView(TestingAPI)

-(void)setController:(id<BookmarkButtonControllerProtocol>)controller {
  controller_ = controller;
}

-(BOOL)dropIndicatorShown {
  return dropIndicatorShown_;
}

-(CGFloat)dropIndicatorPosition {
  return dropIndicatorPosition_;
}

@end

namespace {

// Some values used for mocks and fakes.
const CGFloat kFakeIndicatorPos = 7.0;
const NSPoint kPoint = {10, 10};

class BookmarkBarFolderViewTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    view_.reset([[BookmarkBarFolderView alloc] init]);
    mock_controller_.reset([GetMockController(YES) retain]);
    [view_ awakeFromNib];
    [view_ setController:mock_controller_];
  }

  virtual void TearDown() {
    [mock_controller_ verify];
    CocoaTest::TearDown();
  }

  id GetFakePasteboardForType(NSString* dataType) {
    id pasteboard = [OCMockObject mockForClass:[NSPasteboard class]];
    [[[pasteboard stub] andReturn:[NSData data]] dataForType:dataType];
    [[[pasteboard stub] andReturn:nil] dataForType:OCMOCK_ANY];
    [[[pasteboard stub] andReturnBool:YES] containsURLData];
    [[pasteboard stub] getURLs:[OCMArg setTo:nil]
                     andTitles:[OCMArg setTo:nil]
           convertingFilenames:YES];
    return pasteboard;
  }

  id GetFakeDragInfoForType(NSString* dataType) {
    // Need something non-nil to return as the draggingSource.
    id source = [OCMockObject mockForClass:[NSObject class]];
    id drag_info = [OCMockObject mockForProtocol:@protocol(NSDraggingInfo)];
    id pasteboard = GetFakePasteboardForType(dataType);
    [[[drag_info stub] andReturn:pasteboard] draggingPasteboard];
    [[[drag_info stub] andReturnNSPoint:kPoint] draggingLocation];
    [[[drag_info stub] andReturn:source] draggingSource];
    [[[drag_info stub]
      andReturnUnsignedInteger:NSDragOperationCopy | NSDragOperationMove]
     draggingSourceOperationMask];
    return drag_info;
  }

  id GetMockController(BOOL show_indicator) {
    id mock_controller
        = [OCMockObject mockForClass:[BookmarkBarFolderController class]];
   [[[mock_controller stub] andReturnBool:YES]
     draggingAllowed:OCMOCK_ANY];
    [[[mock_controller stub] andReturnBool:show_indicator]
     shouldShowIndicatorShownForPoint:kPoint];
    [[[mock_controller stub] andReturnFloat:kFakeIndicatorPos]
     indicatorPosForDragToPoint:kPoint];
    return mock_controller;
  }

  scoped_nsobject<id> mock_controller_;
  scoped_nsobject<BookmarkBarFolderView> view_;
};

TEST_F(BookmarkBarFolderViewTest, BookmarkButtonDragAndDrop) {
  id drag_info = GetFakeDragInfoForType(kBookmarkButtonDragType);
  [[[mock_controller_ expect] andReturnUnsignedInteger:NSDragOperationNone]
   draggingEntered:drag_info];
  [[[mock_controller_ expect] andReturnBool:NO] dragBookmarkData:drag_info];
  [[[mock_controller_ expect] andReturnBool:YES] dragButton:OCMOCK_ANY
                                                         to:kPoint
                                                       copy:NO];

  EXPECT_EQ([view_ draggingEntered:drag_info], NSDragOperationMove);
  EXPECT_TRUE([view_ performDragOperation:drag_info]);
}

TEST_F(BookmarkBarFolderViewTest, URLDragAndDrop) {
  NSArray* dragTypes = [URLDropTargetHandler handledDragTypes];
  for (NSString* type in dragTypes) {
    id drag_info = GetFakeDragInfoForType(type);
    [[[mock_controller_ expect] andReturnUnsignedInteger:NSDragOperationNone]
     draggingEntered:drag_info];
    [[[mock_controller_ expect] andReturnBool:NO] dragBookmarkData:drag_info];
    [[[mock_controller_ expect] andReturnBool:YES] addURLs:OCMOCK_ANY
                                                withTitles:OCMOCK_ANY
                                                        at:kPoint];
    EXPECT_EQ([view_ draggingEntered:drag_info], NSDragOperationMove);
    EXPECT_TRUE([view_ performDragOperation:drag_info]);
    [mock_controller_ verify];
  }
}

TEST_F(BookmarkBarFolderViewTest, BookmarkButtonDropIndicator) {
  id drag_info = GetFakeDragInfoForType(kBookmarkButtonDragType);
  [[[mock_controller_ expect] andReturnUnsignedInteger:NSDragOperationNone]
   draggingEntered:drag_info];
  EXPECT_EQ([view_ draggingEntered:drag_info], NSDragOperationMove);
  [mock_controller_ verify];
  EXPECT_TRUE([view_ dropIndicatorShown]);
  EXPECT_EQ([view_ dropIndicatorPosition], kFakeIndicatorPos);
  mock_controller_.reset([GetMockController(NO) retain]);
  [view_ setController:mock_controller_];
  [[[mock_controller_ expect] andReturnUnsignedInteger:NSDragOperationNone]
   draggingEntered:drag_info];
  EXPECT_EQ([view_ draggingEntered:drag_info], NSDragOperationMove);
  EXPECT_FALSE([view_ dropIndicatorShown]);
}

}  // namespace
