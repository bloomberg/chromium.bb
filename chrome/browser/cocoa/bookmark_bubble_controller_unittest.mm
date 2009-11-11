// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bubble_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface BBDelegate : NSObject<BookmarkBubbleControllerDelegate> {
 @private
  BOOL windowClosed_;
  int edits_;
}

@property (readonly) int edits;
@property (readonly) BOOL windowClosed;

@end

@implementation BBDelegate

@synthesize edits = edits_;
@synthesize windowClosed = windowClosed_;

- (void)dealloc {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self];
  [super dealloc];
}

- (NSPoint)topLeftForBubble {
  return NSMakePoint(10, 300);
}

- (void)editBookmarkNode:(const BookmarkNode*)node {
  edits_++;
}

- (void)setWindowController:(NSWindowController *)controller {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self];
  if (controller) {
    [nc addObserver:self
           selector:@selector(windowWillClose:)
               name:NSWindowWillCloseNotification
             object:[controller window]];
  }
  windowClosed_ = NO;
}

- (void)windowWillClose:(NSNotification*)notification {
  windowClosed_ = YES;
}
@end

namespace {

class BookmarkBubbleControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  scoped_nsobject<BBDelegate> delegate_;
  BookmarkBubbleController* controller_;

  BookmarkBubbleControllerTest() : controller_(nil) {
    delegate_.reset([[BBDelegate alloc] init]);
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  // Returns a controller but ownership not transferred.
  // Only one of these will be valid at a time.
  BookmarkBubbleController* ControllerForNode(const BookmarkNode* node) {
    if (controller_)
      [controller_ close];
    controller_ = [[BookmarkBubbleController alloc]
                       initWithDelegate:delegate_.get()
                       parentWindow:test_window()
                       topLeftForBubble:[delegate_ topLeftForBubble]
                       model:helper_.profile()->GetBookmarkModel()
                       node:node
                       alreadyBookmarked:YES];
    EXPECT_TRUE([controller_ window]);
    [delegate_ setWindowController:controller_];
    return controller_;
  }

  BookmarkModel* GetBookmarkModel() {
    return helper_.profile()->GetBookmarkModel();
  }
};

// Confirm basics about the bubble window (e.g. that it is inside the
// parent window)
TEST_F(BookmarkBubbleControllerTest, TestBubbleWindow) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  NSWindow* window = [controller window];
  EXPECT_TRUE(window);
  EXPECT_TRUE(NSContainsRect([test_window() frame],
                             [window frame]));
}

// Confirm population of folder list
TEST_F(BookmarkBubbleControllerTest, TestFillInFolder) {
  // Create some folders, including a nested folder
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node1 = model->AddGroup(bookmarkBarNode, 0, L"one");
  EXPECT_TRUE(node1);
  const BookmarkNode* node2 = model->AddGroup(bookmarkBarNode, 1, L"two");
  EXPECT_TRUE(node2);
  const BookmarkNode* node3 = model->AddGroup(bookmarkBarNode, 2, L"three");
  EXPECT_TRUE(node3);
  const BookmarkNode* node4 = model->AddGroup(node2, 0, L"sub");
  EXPECT_TRUE(node4);
  const BookmarkNode* node5 =
      model->AddURL(node1, 0, L"title1", GURL("http://www.google.com"));
  EXPECT_TRUE(node5);
  const BookmarkNode* node6 =
      model->AddURL(node3, 0, L"title2", GURL("http://www.google.com"));
  EXPECT_TRUE(node6);
  const BookmarkNode* node7 =
      model->AddURL(node4, 0, L"title3", GURL("http://www.google.com/reader"));
  EXPECT_TRUE(node7);

  BookmarkBubbleController* controller = ControllerForNode(node4);
  EXPECT_TRUE(controller);

  NSArray* titles =
      [[[controller folderPopUpButton] itemArray] valueForKey:@"title"];
  EXPECT_TRUE([titles containsObject:@"one"]);
  EXPECT_TRUE([titles containsObject:@"two"]);
  EXPECT_TRUE([titles containsObject:@"three"]);
  EXPECT_TRUE([titles containsObject:@"sub"]);
  EXPECT_FALSE([titles containsObject:@"title1"]);
  EXPECT_FALSE([titles containsObject:@"title2"]);
}

// Click on edit; bubble gets closed.
TEST_F(BookmarkBubbleControllerTest, TestEdit) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  EXPECT_EQ([delegate_ edits], 0);
  EXPECT_FALSE([delegate_ windowClosed]);
  [controller edit:controller];
  EXPECT_EQ([delegate_ edits], 1);
  EXPECT_TRUE([delegate_ windowClosed]);
}

// CallClose; bubble gets closed.
TEST_F(BookmarkBubbleControllerTest, TestClose) {
    BookmarkModel* model = GetBookmarkModel();
    const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                             0,
                                             L"Bookie markie title",
                                             GURL("http://www.google.com"));
  EXPECT_EQ([delegate_ edits], 0);

  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_FALSE([delegate_ windowClosed]);
  [controller ok:controller];
  EXPECT_EQ([delegate_ edits], 0);
  EXPECT_TRUE([delegate_ windowClosed]);
}

// User changes title and parent folder in the UI
TEST_F(BookmarkBubbleControllerTest, TestUserEdit) {
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node = model->AddURL(bookmarkBarNode,
                                           0,
                                           L"short-title",
                                           GURL("http://www.google.com"));
  const BookmarkNode* grandma = model->AddGroup(bookmarkBarNode, 0, L"grandma");
  EXPECT_TRUE(grandma);
  const BookmarkNode* grandpa = model->AddGroup(bookmarkBarNode, 0, L"grandpa");
  EXPECT_TRUE(grandpa);

  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  // simulate a user edit
  [controller setTitle:@"oops" parentFolder:grandma];
  [controller edit:controller];

  // Make sure bookmark has changed
  EXPECT_EQ(node->GetTitle(), L"oops");
  EXPECT_EQ(node->GetParent()->GetTitle(), L"grandma");
}

// Confirm happiness with parent nodes that have the same name.
TEST_F(BookmarkBubbleControllerTest, TestNewParentSameName) {
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  for (int i=0; i<2; i++) {
    const BookmarkNode* node = model->AddURL(bookmarkBarNode,
                                             0,
                                             L"short-title",
                                             GURL("http://www.google.com"));
    EXPECT_TRUE(node);
    const BookmarkNode* group = model->AddGroup(bookmarkBarNode, 0, L"NAME");
    EXPECT_TRUE(group);
    group = model->AddGroup(bookmarkBarNode, 0, L"NAME");
    EXPECT_TRUE(group);
    group = model->AddGroup(bookmarkBarNode, 0, L"NAME");
    EXPECT_TRUE(group);
    BookmarkBubbleController* controller = ControllerForNode(node);
    EXPECT_TRUE(controller);

    // simulate a user edit
    [controller setParentFolderSelection:bookmarkBarNode->GetChild(i)];
    [controller edit:controller];

    // Make sure bookmark has changed, and that the parent is what we
    // expect.  This proves nobody did searching based on name.
    EXPECT_EQ(node->GetParent(), bookmarkBarNode->GetChild(i));
  }
}

// Confirm happiness with nodes with the same Name
TEST_F(BookmarkBubbleControllerTest, TestDuplicateNodeNames) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node1 = model->AddGroup(bookmarkBarNode, 0, L"NAME");
  EXPECT_TRUE(node1);
  const BookmarkNode* node2 = model->AddGroup(bookmarkBarNode, 0, L"NAME");
  EXPECT_TRUE(node2);
  BookmarkBubbleController* controller = ControllerForNode(bookmarkBarNode);
  EXPECT_TRUE(controller);

  NSPopUpButton* button = [controller folderPopUpButton];
  [controller setParentFolderSelection:node1];
  NSMenuItem* item = [button selectedItem];
  id itemObject = [item representedObject];
  EXPECT_TRUE([itemObject isEqual:[NSValue valueWithPointer:node1]]);
  [controller setParentFolderSelection:node2];
  item = [button selectedItem];
  itemObject = [item representedObject];
  EXPECT_TRUE([itemObject isEqual:[NSValue valueWithPointer:node2]]);
}

// Click the "remove" button
TEST_F(BookmarkBubbleControllerTest, TestRemove) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_TRUE(model->IsBookmarked(gurl));

  [controller remove:controller];
  EXPECT_FALSE(model->IsBookmarked(gurl));
  EXPECT_TRUE([delegate_ windowClosed]);
}

// Confirm picking "choose another folder" caused edit: to be called.
TEST_F(BookmarkBubbleControllerTest, PopUpSelectionChanged) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0, L"super-title",
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  NSPopUpButton* button = [controller folderPopUpButton];
  [button selectItemWithTitle:[[controller class] chooseAnotherFolderString]];
  EXPECT_EQ([delegate_ edits], 0);
  [button sendAction:[button action] to:[button target]];
  EXPECT_EQ([delegate_ edits], 1);
}

// Create a controller that simulates the bookmark just now being created by
// the user clicking the star, then sending the "cancel" command to represent
// them pressing escape. The bookmark should not be there.
TEST_F(BookmarkBubbleControllerTest, EscapeRemovesNewBookmark) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           gurl);
  BookmarkBubbleController* controller =
      [[BookmarkBubbleController alloc]
          initWithDelegate:delegate_.get()
              parentWindow:test_window()
          topLeftForBubble:[delegate_ topLeftForBubble]
                     model:helper_.profile()->GetBookmarkModel()
                      node:node
         alreadyBookmarked:NO];  // The last param is the key difference.
  EXPECT_TRUE([controller window]);
  // Calls release on controller.
  [controller cancel:nil];
  EXPECT_FALSE(model->IsBookmarked(gurl));
}

// Create a controller where the bookmark already existed prior to clicking
// the star and test that sending a cancel command doesn't change the state
// of the bookmark.
TEST_F(BookmarkBubbleControllerTest, EscapeDoesntTouchExistingBookmark) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           L"Bookie markie title",
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  [(id)controller cancel:nil];
  EXPECT_TRUE(model->IsBookmarked(gurl));
}

}  // namespace
