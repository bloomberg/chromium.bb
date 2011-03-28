// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bubble_controller.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "content/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// Watch for bookmark pulse notifications so we can confirm they were sent.
@interface BookmarkPulseObserver : NSObject {
  int notifications_;
}
@property (assign, nonatomic) int notifications;
@end


@implementation BookmarkPulseObserver

@synthesize notifications = notifications_;

- (id)init {
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(pulseBookmarkNotification:)
             name:bookmark_button::kPulseBookmarkButtonNotification
           object:nil];
  }
  return self;
}

- (void)pulseBookmarkNotification:(NSNotificationCenter *)notification {
  notifications_++;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

@end


namespace {

class BookmarkBubbleControllerTest : public CocoaTest {
 public:
  static int edits_;
  BrowserTestHelper helper_;
  BookmarkBubbleController* controller_;

  BookmarkBubbleControllerTest() : controller_(nil) {
    edits_ = 0;
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  // Returns a controller but ownership not transferred.
  // Only one of these will be valid at a time.
  BookmarkBubbleController* ControllerForNode(const BookmarkNode* node) {
    if (controller_ && !IsWindowClosing()) {
      [controller_ close];
      controller_ = nil;
    }
    controller_ = [[BookmarkBubbleController alloc]
                      initWithParentWindow:test_window()
                                     model:helper_.profile()->GetBookmarkModel()
                                      node:node
                         alreadyBookmarked:YES];
    EXPECT_TRUE([controller_ window]);
    // The window must be gone or we'll fail a unit test with windows left open.
    [static_cast<InfoBubbleWindow*>([controller_ window]) setDelayOnClose:NO];
    [controller_ showWindow:nil];
    return controller_;
  }

  BookmarkModel* GetBookmarkModel() {
    return helper_.profile()->GetBookmarkModel();
  }

  bool IsWindowClosing() {
    return [static_cast<InfoBubbleWindow*>([controller_ window]) isClosing];
  }
};

// static
int BookmarkBubbleControllerTest::edits_;

// Confirm basics about the bubble window (e.g. that it is inside the
// parent window)
TEST_F(BookmarkBubbleControllerTest, TestBubbleWindow) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           ASCIIToUTF16("Bookie markie title"),
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  NSWindow* window = [controller window];
  EXPECT_TRUE(window);
  EXPECT_TRUE(NSContainsRect([test_window() frame],
                             [window frame]));
}

// Test that we can handle closing the parent window
TEST_F(BookmarkBubbleControllerTest, TestClosingParentWindow) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           ASCIIToUTF16("Bookie markie title"),
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  NSWindow* window = [controller window];
  EXPECT_TRUE(window);
  base::mac::ScopedNSAutoreleasePool pool;
  [test_window() performClose:NSApp];
}


// Confirm population of folder list
TEST_F(BookmarkBubbleControllerTest, TestFillInFolder) {
  // Create some folders, including a nested folder
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node1 = model->AddFolder(bookmarkBarNode, 0,
                                               ASCIIToUTF16("one"));
  EXPECT_TRUE(node1);
  const BookmarkNode* node2 = model->AddFolder(bookmarkBarNode, 1,
                                               ASCIIToUTF16("two"));
  EXPECT_TRUE(node2);
  const BookmarkNode* node3 = model->AddFolder(bookmarkBarNode, 2,
                                               ASCIIToUTF16("three"));
  EXPECT_TRUE(node3);
  const BookmarkNode* node4 = model->AddFolder(node2, 0, ASCIIToUTF16("sub"));
  EXPECT_TRUE(node4);
  const BookmarkNode* node5 = model->AddURL(node1, 0, ASCIIToUTF16("title1"),
                                            GURL("http://www.google.com"));
  EXPECT_TRUE(node5);
  const BookmarkNode* node6 = model->AddURL(node3, 0, ASCIIToUTF16("title2"),
                                            GURL("http://www.google.com"));
  EXPECT_TRUE(node6);
  const BookmarkNode* node7 = model->AddURL(
      node4, 0, ASCIIToUTF16("title3"), GURL("http://www.google.com/reader"));
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

// Confirm ability to handle folders with blank name.
TEST_F(BookmarkBubbleControllerTest, TestFolderWithBlankName) {
  // Create some folders, including a nested folder
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node1 = model->AddFolder(bookmarkBarNode, 0,
                                               ASCIIToUTF16("one"));
  EXPECT_TRUE(node1);
  const BookmarkNode* node2 = model->AddFolder(bookmarkBarNode, 1,
                                               ASCIIToUTF16(""));
  EXPECT_TRUE(node2);
  const BookmarkNode* node3 = model->AddFolder(bookmarkBarNode, 2,
                                               ASCIIToUTF16("three"));
  EXPECT_TRUE(node3);
  const BookmarkNode* node2_1 = model->AddURL(node2, 0, ASCIIToUTF16("title1"),
                                              GURL("http://www.google.com"));
  EXPECT_TRUE(node2_1);

  BookmarkBubbleController* controller = ControllerForNode(node1);
  EXPECT_TRUE(controller);

  // One of the items should be blank and its node should be node2.
  NSArray* items = [[controller folderPopUpButton] itemArray];
  EXPECT_GT([items count], 4U);
  BOOL blankFolderFound = NO;
  for (NSMenuItem* item in [[controller folderPopUpButton] itemArray]) {
    if ([[item title] length] == 0 &&
        static_cast<const BookmarkNode*>([[item representedObject]
                                          pointerValue]) == node2) {
      blankFolderFound = YES;
      break;
    }
  }
  EXPECT_TRUE(blankFolderFound);
}


// Click on edit; bubble gets closed.
TEST_F(BookmarkBubbleControllerTest, TestEdit) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           ASCIIToUTF16("Bookie markie title"),
                                           GURL("http://www.google.com"));
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  EXPECT_EQ(edits_, 0);
  EXPECT_FALSE(IsWindowClosing());
  [controller edit:controller];
  EXPECT_EQ(edits_, 1);
  EXPECT_TRUE(IsWindowClosing());
}

// CallClose; bubble gets closed.
// Also confirm pulse notifications get sent.
TEST_F(BookmarkBubbleControllerTest, TestClose) {
    BookmarkModel* model = GetBookmarkModel();
    const BookmarkNode* node = model->AddURL(
        model->GetBookmarkBarNode(), 0, ASCIIToUTF16("Bookie markie title"),
        GURL("http://www.google.com"));
  EXPECT_EQ(edits_, 0);

  scoped_nsobject<BookmarkPulseObserver> observer([[BookmarkPulseObserver alloc]
                                                    init]);
  EXPECT_EQ([observer notifications], 0);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_FALSE(IsWindowClosing());
  EXPECT_EQ([observer notifications], 1);
  [controller ok:controller];
  EXPECT_EQ(edits_, 0);
  EXPECT_TRUE(IsWindowClosing());
  EXPECT_EQ([observer notifications], 2);
}

// User changes title and parent folder in the UI
TEST_F(BookmarkBubbleControllerTest, TestUserEdit) {
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node = model->AddURL(bookmarkBarNode,
                                           0,
                                           ASCIIToUTF16("short-title"),
                                           GURL("http://www.google.com"));
  const BookmarkNode* grandma = model->AddFolder(bookmarkBarNode, 0,
                                                 ASCIIToUTF16("grandma"));
  EXPECT_TRUE(grandma);
  const BookmarkNode* grandpa = model->AddFolder(bookmarkBarNode, 0,
                                                 ASCIIToUTF16("grandpa"));
  EXPECT_TRUE(grandpa);

  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  // simulate a user edit
  [controller setTitle:@"oops" parentFolder:grandma];
  [controller edit:controller];

  // Make sure bookmark has changed
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("oops"));
  EXPECT_EQ(node->parent()->GetTitle(), ASCIIToUTF16("grandma"));
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
                                             ASCIIToUTF16("short-title"),
                                             GURL("http://www.google.com"));
    EXPECT_TRUE(node);
    const BookmarkNode* folder = model->AddFolder(bookmarkBarNode, 0,
                                                 ASCIIToUTF16("NAME"));
    EXPECT_TRUE(folder);
    folder = model->AddFolder(bookmarkBarNode, 0, ASCIIToUTF16("NAME"));
    EXPECT_TRUE(folder);
    folder = model->AddFolder(bookmarkBarNode, 0, ASCIIToUTF16("NAME"));
    EXPECT_TRUE(folder);
    BookmarkBubbleController* controller = ControllerForNode(node);
    EXPECT_TRUE(controller);

    // simulate a user edit
    [controller setParentFolderSelection:bookmarkBarNode->GetChild(i)];
    [controller edit:controller];

    // Make sure bookmark has changed, and that the parent is what we
    // expect.  This proves nobody did searching based on name.
    EXPECT_EQ(node->parent(), bookmarkBarNode->GetChild(i));
  }
}

// Confirm happiness with nodes with the same Name
TEST_F(BookmarkBubbleControllerTest, TestDuplicateNodeNames) {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node1 = model->AddFolder(bookmarkBarNode, 0,
                                               ASCIIToUTF16("NAME"));
  EXPECT_TRUE(node1);
  const BookmarkNode* node2 = model->AddFolder(bookmarkBarNode, 0,
                                               ASCIIToUTF16("NAME"));
  EXPECT_TRUE(node2);
  BookmarkBubbleController* controller = ControllerForNode(bookmarkBarNode);
  EXPECT_TRUE(controller);

  NSPopUpButton* button = [controller folderPopUpButton];
  [controller setParentFolderSelection:node1];
  NSMenuItem* item = [button selectedItem];
  id itemObject = [item representedObject];
  EXPECT_NSEQ([NSValue valueWithPointer:node1], itemObject);
  [controller setParentFolderSelection:node2];
  item = [button selectedItem];
  itemObject = [item representedObject];
  EXPECT_NSEQ([NSValue valueWithPointer:node2], itemObject);
}

// Click the "remove" button
TEST_F(BookmarkBubbleControllerTest, TestRemove) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           ASCIIToUTF16("Bookie markie title"),
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_TRUE(model->IsBookmarked(gurl));

  [controller remove:controller];
  EXPECT_FALSE(model->IsBookmarked(gurl));
  EXPECT_TRUE(IsWindowClosing());
}

// Confirm picking "choose another folder" caused edit: to be called.
TEST_F(BookmarkBubbleControllerTest, PopUpSelectionChanged) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0, ASCIIToUTF16("super-title"),
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  NSPopUpButton* button = [controller folderPopUpButton];
  [button selectItemWithTitle:[[controller class] chooseAnotherFolderString]];
  EXPECT_EQ(edits_, 0);
  [button sendAction:[button action] to:[button target]];
  EXPECT_EQ(edits_, 1);
}

// Create a controller that simulates the bookmark just now being created by
// the user clicking the star, then sending the "cancel" command to represent
// them pressing escape. The bookmark should not be there.
TEST_F(BookmarkBubbleControllerTest, EscapeRemovesNewBookmark) {
  BookmarkModel* model = GetBookmarkModel();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           ASCIIToUTF16("Bookie markie title"),
                                           gurl);
  BookmarkBubbleController* controller =
      [[BookmarkBubbleController alloc]
          initWithParentWindow:test_window()
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
                                           ASCIIToUTF16("Bookie markie title"),
                                           gurl);
  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);

  [(id)controller cancel:nil];
  EXPECT_TRUE(model->IsBookmarked(gurl));
}

// Confirm indentation of items in pop-up menu
TEST_F(BookmarkBubbleControllerTest, TestMenuIndentation) {
  // Create some folders, including a nested folder
  BookmarkModel* model = GetBookmarkModel();
  EXPECT_TRUE(model);
  const BookmarkNode* bookmarkBarNode = model->GetBookmarkBarNode();
  EXPECT_TRUE(bookmarkBarNode);
  const BookmarkNode* node1 = model->AddFolder(bookmarkBarNode, 0,
                                               ASCIIToUTF16("one"));
  EXPECT_TRUE(node1);
  const BookmarkNode* node2 = model->AddFolder(bookmarkBarNode, 1,
                                               ASCIIToUTF16("two"));
  EXPECT_TRUE(node2);
  const BookmarkNode* node2_1 = model->AddFolder(node2, 0,
                                                 ASCIIToUTF16("two dot one"));
  EXPECT_TRUE(node2_1);
  const BookmarkNode* node3 = model->AddFolder(bookmarkBarNode, 2,
                                               ASCIIToUTF16("three"));
  EXPECT_TRUE(node3);

  BookmarkBubbleController* controller = ControllerForNode(node1);
  EXPECT_TRUE(controller);

  // Compare the menu item indents against expectations.
  static const int kExpectedIndent[] = {0, 1, 1, 2, 1, 0};
  NSArray* items = [[controller folderPopUpButton] itemArray];
  ASSERT_GE([items count], 6U);
  for(int itemNo = 0; itemNo < 6; itemNo++) {
    NSMenuItem* item = [items objectAtIndex:itemNo];
    EXPECT_EQ(kExpectedIndent[itemNo], [item indentationLevel])
        << "Unexpected indent for menu item #" << itemNo;
  }
}

// Confirm bubble goes away when a new tab is created.
TEST_F(BookmarkBubbleControllerTest, BubbleGoesAwayOnNewTab) {

  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0,
                                           ASCIIToUTF16("Bookie markie title"),
                                           GURL("http://www.google.com"));
  EXPECT_EQ(edits_, 0);

  BookmarkBubbleController* controller = ControllerForNode(node);
  EXPECT_TRUE(controller);
  EXPECT_FALSE(IsWindowClosing());

  // We can't actually create a new tab here, e.g.
  //   helper_.browser()->AddTabWithURL(...);
  // Many of our browser objects (Browser, Profile, RequestContext)
  // are "just enough" to run tests without being complete.  Instead
  // we fake the notification that would be triggered by a tab
  // creation.
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_CONNECTED,
      Source<TabContentsDelegate>(NULL),
      Details<TabContents>(NULL));

  // Confirm bubble going bye-bye.
  EXPECT_TRUE(IsWindowClosing());
}


}  // namespace

@implementation NSApplication (BookmarkBubbleUnitTest)
// Add handler for the editBookmarkNode: action to NSApp for testing purposes.
// Normally this would be sent up the responder tree correctly, but since
// tests run in the background, key window and main window are never set on
// NSApplication. Adding it to NSApplication directly removes the need for
// worrying about what the current window with focus is.
- (void)editBookmarkNode:(id)sender {
  EXPECT_TRUE([sender respondsToSelector:@selector(node)]);
  BookmarkBubbleControllerTest::edits_++;
}

@end
