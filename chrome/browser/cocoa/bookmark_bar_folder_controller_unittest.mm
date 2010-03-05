// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface BookmarkBarFolderControllerPong : BookmarkBarFolderController {
  BOOL childFolderWillShow_;
  BOOL childFolderWillClose_;
}
@property(readonly) BOOL childFolderWillShow;
@property(readonly) BOOL childFolderWillClose;
@end

@implementation BookmarkBarFolderControllerPong
@synthesize childFolderWillShow = childFolderWillShow_;
@synthesize childFolderWillClose = childFolderWillClose_;

- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child {
  childFolderWillShow_ = YES;
}

- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child {
  childFolderWillClose_ = YES;
}
@end

class BookmarkBarFolderControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkBarController> parentBarController_;

  BookmarkBarFolderControllerTest() {
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    const BookmarkNode* parent = model->GetBookmarkBarNode();
    const BookmarkNode* folderA = model->AddGroup(parent,
                                                  parent->GetChildCount(),
                                                  L"group");
    model->AddGroup(parent, parent->GetChildCount(),
                    L"sibbling group");
    const BookmarkNode* folderB = model->AddGroup(folderA,
                                                  folderA->GetChildCount(),
                                                  L"subgroup");
    model->AddURL(folderA, folderA->GetChildCount(), L"title a",
                  GURL("http://www.google.com/a"));
    model->AddURL(folderA, folderA->GetChildCount(),
                  L"title super duper long long whoa momma title you betcha",
                  GURL("http://www.google.com/b"));
    model->AddURL(folderB, folderB->GetChildCount(), L"t",
                  GURL("http://www.google.com/c"));

    parentBarController_.reset(
      [[BookmarkBarController alloc]
          initWithBrowser:helper_.browser()
             initialWidth:300
                 delegate:nil
           resizeDelegate:nil]);
    [parentBarController_ loaded:model];
  }

  // Return a simple BookmarkBarFolderController.
  BookmarkBarFolderController* SimpleBookmarkBarFolderController() {
    BookmarkButton* parentButton = [[parentBarController_ buttons]
                                     objectAtIndex:0];
    return [[BookmarkBarFolderControllerPong alloc]
               initWithParentButton:parentButton
                   parentController:parentBarController_];
  }

};

TEST_F(BookmarkBarFolderControllerTest, InitCreateAndDelete) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());

  // Make sure none of the buttons overlap, and that all are inside
  // the content frame.
  NSArray* buttons = [bbfc buttons];
  EXPECT_TRUE([buttons count]);
  for (unsigned int i = 0; i < ([buttons count]-1); i++) {
    EXPECT_FALSE(NSContainsRect([[buttons objectAtIndex:i] frame],
                              [[buttons objectAtIndex:i+1] frame]));
  }
  for (BookmarkButton* button in buttons) {
    NSRect r = [[bbfc mainView] convertRect:[button frame] fromView:button];
    EXPECT_TRUE(NSContainsRect([[bbfc mainView] frame], r));
  }

  // Confirm folder buttons have no tooltip.  The important thing
  // really is that we insure folders and non-folders are treated
  // differently; not sure of any other generic way to do this.
  for (BookmarkButton* button in buttons) {
    if ([button isFolder])
      EXPECT_FALSE([button toolTip]);
    else
      EXPECT_TRUE([button toolTip]);
  }
}

// Make sure closing of the window releases the controller.
// (e.g. valgrind shouldn't complain if we do this).
TEST_F(BookmarkBarFolderControllerTest, ReleaseOnClose) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());

  [bbfc retain];  // stop the scoped_nsobject from doing anything
  [[bbfc window] close];  // trigger an autorelease of bbfc.get()
}

TEST_F(BookmarkBarFolderControllerTest, Position) {
 BookmarkButton* parentButton = [[parentBarController_ buttons]
                                   objectAtIndex:0];
  EXPECT_TRUE(parentButton);

  // If parent is a BookmarkBarController, grow down.
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset([[BookmarkBarFolderController alloc]
               initWithParentButton:parentButton
                   parentController:parentBarController_]);
  NSPoint pt = [bbfc windowTopLeft];
  EXPECT_EQ(pt.y, NSMinY([[parentBarController_ view] frame]));

  // If parent is a BookmarkBarFolderController, grow right.
  scoped_nsobject<BookmarkBarFolderController> bbfc2;
  bbfc2.reset([[BookmarkBarFolderController alloc]
                initWithParentButton:[[bbfc buttons] objectAtIndex:0]
                    parentController:bbfc.get()]);
  pt = [bbfc2 windowTopLeft];
  EXPECT_EQ(pt.x, NSMaxX([[[bbfc.get() window] contentView] frame]));
}

TEST_F(BookmarkBarFolderControllerTest, DropDestination) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());

  // Confirm "off the top" and "off the bottom" match no buttons.
  NSPoint p = NSMakePoint(NSMidX([[bbfc mainView] frame]), 10000);
  EXPECT_FALSE([bbfc buttonForDroppingOnAtPoint:p]);
  EXPECT_TRUE([bbfc shouldShowIndicatorShownForPoint:p]);
  p = NSMakePoint(NSMidX([[bbfc mainView] frame]), -1);
  EXPECT_FALSE([bbfc buttonForDroppingOnAtPoint:p]);
  EXPECT_TRUE([bbfc shouldShowIndicatorShownForPoint:p]);

  // Confirm "right in the center" (give or take a pixel) is a match,
  // and confirm "just barely in the button" is not.  Anything more
  // specific seems likely to be tweaked.
  for (BookmarkButton* button in [bbfc buttons]) {
    CGFloat x = NSMidX([button frame]);
    CGFloat y = NSMidY([button frame]);
    // Somewhere near the center: a match (but only if a folder!)
    if ([button isFolder]) {
      EXPECT_EQ(button,
                [bbfc buttonForDroppingOnAtPoint:NSMakePoint(x-1, y+1)]);
      EXPECT_EQ(button,
                [bbfc buttonForDroppingOnAtPoint:NSMakePoint(x+1, y-1)]);
      EXPECT_FALSE([bbfc shouldShowIndicatorShownForPoint:NSMakePoint(x, y)]);;
    } else {
      // If not a folder we don't drop into it.
      EXPECT_FALSE([bbfc buttonForDroppingOnAtPoint:NSMakePoint(x-1, y+1)]);
      EXPECT_FALSE([bbfc buttonForDroppingOnAtPoint:NSMakePoint(x+1, y-1)]);
      EXPECT_TRUE([bbfc shouldShowIndicatorShownForPoint:NSMakePoint(x, y)]);;
    }


    // On some corners: NOT a match.  Confirm that the indicator
    // position for these two points is NOT the same.
    BookmarkButton* dragButton = [[bbfc buttons] lastObject];
    x = NSMinX([button frame]);
    y = NSMinY([button frame]);
    CGFloat pos1 = [bbfc indicatorPosForDragOfButton:dragButton
                                             toPoint:NSMakePoint(x, y)];
    EXPECT_NE(button,
              [bbfc buttonForDroppingOnAtPoint:NSMakePoint(x, y)]);
    x = NSMaxX([button frame]);
    y = NSMaxY([button frame]);
    CGFloat pos2 = [bbfc indicatorPosForDragOfButton:dragButton
                                             toPoint:NSMakePoint(x, y)];
    EXPECT_NE(button,
              [bbfc buttonForDroppingOnAtPoint:NSMakePoint(x, y)]);
    if (dragButton != button) {
      EXPECT_NE(pos1, pos2);
    }
  }
}

TEST_F(BookmarkBarFolderControllerTest, OpenFolder) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());

  EXPECT_FALSE([bbfc folderController]);
  [bbfc openBookmarkFolderFromButton:[[bbfc buttons] objectAtIndex:0]];
  id controller = [bbfc folderController];
  EXPECT_TRUE(controller);

  // Open the same one --> no change.
  [bbfc openBookmarkFolderFromButton:[[bbfc buttons] objectAtIndex:0]];
  EXPECT_EQ(controller, [bbfc folderController]);

  // Open a new one --> change.
  [bbfc openBookmarkFolderFromButton:[[bbfc buttons] objectAtIndex:1]];
  EXPECT_NE(controller, [bbfc folderController]);

  // Close it --> all gone!
  [bbfc closeBookmarkFolder:nil];
  EXPECT_FALSE([bbfc folderController]);
}

TEST_F(BookmarkBarFolderControllerTest, ChildFolderCallbacks) {
  scoped_nsobject<BookmarkBarFolderControllerPong> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());

  EXPECT_FALSE([bbfc childFolderWillShow]);
  [bbfc openBookmarkFolderFromButton:[[bbfc buttons] objectAtIndex:0]];
  EXPECT_TRUE([bbfc childFolderWillShow]);

  EXPECT_FALSE([bbfc childFolderWillClose]);
  [bbfc closeBookmarkFolder:nil];
  EXPECT_TRUE([bbfc childFolderWillClose]);
}

// TODO(jrg): draggingEntered: and draggingExited: trigger timers so
// they are hard to test.  Factor out "fire timers" into routines
// which can be overridden to fire immediately to make behavior
// confirmable.
// There is a similar problem with mouseEnteredButton: and
// mouseExitedButton:.
