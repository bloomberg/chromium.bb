// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_button_cell.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Add a redirect to make testing easier.
@interface BookmarkBarFolderController(MakeTestingEasier)
- (IBAction)openBookmarkFolderFromButton:(id)sender;
@end

@implementation BookmarkBarFolderController(MakeTestingEasier)
- (IBAction)openBookmarkFolderFromButton:(id)sender {
  [[self folderTarget] openBookmarkFolderFromButton:sender];
}
@end

// Don't use a high window level when running unit tests -- it'll
// interfere with anything else you are working on.
@interface BookmarkBarFolderControllerLow : BookmarkBarFolderController {
  BOOL realTopLeft_;  // Use the real windowTopLeft call?
}
@property BOOL realTopLeft;
@end


@implementation BookmarkBarFolderControllerLow

@synthesize realTopLeft = realTopLeft_;

- (void)configureWindowLevel {
  // Intentionally empty.
}

- (NSPoint)windowTopLeft {
  return realTopLeft_ ? [super windowTopLeft] : NSMakePoint(200,200);
}

@end


@interface BookmarkBarFolderControllerPong : BookmarkBarFolderControllerLow {
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

// We don't have a real BookmarkBarController as our parent root so
// we fake this one out.
- (void)closeAllBookmarkFolders {
  [self closeBookmarkFolder:self];
}

@end

class BookmarkBarFolderControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkBarController> parentBarController_;
  const BookmarkNode* folderA_;  // owned by model
  const BookmarkNode* longTitleNode_;  // owned by model

  BookmarkBarFolderControllerTest() {
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    const BookmarkNode* parent = model->GetBookmarkBarNode();
    const BookmarkNode* folderA = model->AddGroup(parent,
                                                  parent->GetChildCount(),
                                                  L"group");
    folderA_ = folderA;
    model->AddGroup(parent, parent->GetChildCount(),
                    L"sibbling group");
    const BookmarkNode* folderB = model->AddGroup(folderA,
                                                  folderA->GetChildCount(),
                                                  L"subgroup 1");
    model->AddGroup(folderA,
                    folderA->GetChildCount(),
                    L"subgroup 2");
    model->AddURL(folderA, folderA->GetChildCount(), L"title a",
                  GURL("http://www.google.com/a"));
    longTitleNode_ = model->AddURL(
      folderA, folderA->GetChildCount(),
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
    [[test_window() contentView] addSubview:[parentBarController_ view]];
  }

  // Remove the bookmark with the long title.
  void RemoveLongTitleNode() {
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    model->Remove(longTitleNode_->GetParent(),
                  longTitleNode_->GetParent()->IndexOfChild(longTitleNode_));
  }

  // Add LOTS of nodes to our model if needed (e.g. scrolling).
  void AddLotsOfNodes() {
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    for (int i = 0; i < 150; i++) {
      model->AddURL(folderA_, folderA_->GetChildCount(), L"repeated title",
                    GURL("http://www.google.com/repeated/url"));
    }
  }


  // Return a simple BookmarkBarFolderController.
  BookmarkBarFolderController* SimpleBookmarkBarFolderController() {
    BookmarkButton* parentButton = [[parentBarController_ buttons]
                                     objectAtIndex:0];
    BookmarkBarFolderController* c =
      [[BookmarkBarFolderControllerPong alloc]
               initWithParentButton:parentButton
                   parentController:nil
                      barController:parentBarController_];
    [c window];  // Force nib load.
    return c;
  }

};

TEST_F(BookmarkBarFolderControllerTest, InitCreateAndDelete) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());

  // Make sure none of the buttons overlap, that all are inside
  // the content frame, and their cells are of the proper class.
  NSArray* buttons = [bbfc buttons];
  EXPECT_TRUE([buttons count]);
  for (unsigned int i = 0; i < ([buttons count]-1); i++) {
    EXPECT_FALSE(NSContainsRect([[buttons objectAtIndex:i] frame],
                              [[buttons objectAtIndex:i+1] frame]));
  }
  Class cellClass = [BookmarkBarFolderButtonCell class];
  for (BookmarkButton* button in buttons) {
    NSRect r = [[bbfc mainView] convertRect:[button frame] fromView:button];
    EXPECT_TRUE(NSContainsRect([[bbfc mainView] frame], r));
    EXPECT_TRUE([[button cell] isKindOfClass:cellClass]);
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
  scoped_nsobject<BookmarkBarFolderControllerLow> bbfc;
  bbfc.reset([[BookmarkBarFolderControllerLow alloc]
               initWithParentButton:parentButton
                   parentController:nil
                      barController:parentBarController_]);
  [bbfc window];
  [bbfc setRealTopLeft:YES];
  NSPoint pt = [bbfc windowTopLeft];  // screen coords
  NSPoint buttonOriginInScreen =
      [[parentButton window]
        convertBaseToScreen:[parentButton
                              convertRectToBase:[parentButton frame]].origin];
  // Within margin
  EXPECT_LE(abs(pt.x - buttonOriginInScreen.x), 2);
  EXPECT_LE(abs(pt.y - buttonOriginInScreen.y), 2);

  // If parent is a BookmarkBarFolderController, grow right.
  scoped_nsobject<BookmarkBarFolderControllerLow> bbfc2;
  bbfc2.reset([[BookmarkBarFolderControllerLow alloc]
                initWithParentButton:[[bbfc buttons] objectAtIndex:0]
                    parentController:bbfc.get()
                       barController:parentBarController_]);
  [bbfc2 window];
  [bbfc2 setRealTopLeft:YES];
  pt = [bbfc2 windowTopLeft];
  EXPECT_EQ(pt.x, NSMaxX([[bbfc.get() window] frame]));
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
  // specific seems likely to be tweaked.  We don't loop over all
  // buttons because the scroll view makes them not visible.
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
  }
}

TEST_F(BookmarkBarFolderControllerTest, OpenFolder) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());

  EXPECT_FALSE([bbfc folderController]);
  BookmarkButton* button = [[bbfc buttons] objectAtIndex:0];
  [bbfc openBookmarkFolderFromButton:button];
  id controller = [bbfc folderController];
  EXPECT_TRUE(controller);
  EXPECT_EQ([controller parentButton], button);

  // Click the same one --> it gets closed.
  [bbfc openBookmarkFolderFromButton:[[bbfc buttons] objectAtIndex:0]];
  EXPECT_FALSE([bbfc folderController]);

  // Open a new one --> change.
  [bbfc openBookmarkFolderFromButton:[[bbfc buttons] objectAtIndex:1]];
  EXPECT_NE(controller, [bbfc folderController]);
  EXPECT_NE([[bbfc folderController] parentButton], button);

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

// Make sure bookmark folders have variable widths.
TEST_F(BookmarkBarFolderControllerTest, ChildFolderWidth) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;

  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());
  [bbfc showWindow:bbfc.get()];
  CGFloat wideWidth = NSWidth([[bbfc window] frame]);

  RemoveLongTitleNode();
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());
  CGFloat thinWidth = NSWidth([[bbfc window] frame]);

  // Make sure window size changed as expected.
  EXPECT_GT(wideWidth, thinWidth);
}

// Simple scrolling tests.
TEST_F(BookmarkBarFolderControllerTest, SimpleScroll) {
  scoped_nsobject<BookmarkBarFolderController> bbfc;

  AddLotsOfNodes();
  bbfc.reset(SimpleBookmarkBarFolderController());
  EXPECT_TRUE(bbfc.get());
  [bbfc showWindow:bbfc.get()];

  // Make sure the window fits on the screen.
  EXPECT_LT(NSHeight([[bbfc window] frame]),
            NSHeight([[NSScreen mainScreen] frame]));

  // Scroll it up.  Make sure the window has gotten bigger each time.
  // Also, for each scroll, make sure our hit test finds a new button
  // (to confirm the content area changed).
  NSView* savedHit = nil;
  for (int i=0; i<3; i++) {
    CGFloat height = NSHeight([[bbfc window] frame]);
    [bbfc performOneScroll:60];
    EXPECT_GT(NSHeight([[bbfc window] frame]), height);
    NSView* hit = [[[bbfc window] contentView] hitTest:NSMakePoint(10, 10)];
    EXPECT_NE(hit, savedHit);
    savedHit = hit;
  }

  // Keep scrolling up; make sure we never get bigger than the screen.
  // Also confirm we never scroll the window off the screen.
  NSRect screenFrame = [[NSScreen mainScreen] frame];
  for (int i=0; i<100; i++) {
    [bbfc performOneScroll:60];
    EXPECT_TRUE(NSContainsRect(screenFrame,
                               [[bbfc window] frame]));
  }

  // Now scroll down and make sure the window size does not change.
  // Also confirm we never scroll the window off the screen the other
  // way.
  for (int i=0; i<200; i++) {
    CGFloat height = NSHeight([[bbfc window] frame]);
    [bbfc performOneScroll:-60];
    EXPECT_EQ(height, NSHeight([[bbfc window] frame]));
    EXPECT_TRUE(NSContainsRect(screenFrame,
                               [[bbfc window] frame]));
  }
}

// TODO(jrg): draggingEntered: and draggingExited: trigger timers so
// they are hard to test.  Factor out "fire timers" into routines
// which can be overridden to fire immediately to make behavior
// confirmable.
// There is a similar problem with mouseEnteredButton: and
// mouseExitedButton:.
