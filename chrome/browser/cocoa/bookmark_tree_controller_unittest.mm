// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Mac WebKit uses this type, declared in WebKit/mac/History/WebURLsWithTitles.h
static NSString* const WebURLsWithTitlesPboardType =
    @"WebURLsWithTitlesPboardType";

namespace {

class BookmarkTreeControllerTest : public CocoaTest {
 public:
  void SetUp() {
    CocoaTest::SetUp();
    pasteboard_ = [NSPasteboard pasteboardWithUniqueName];
    manager_ = [BookmarkManagerController showBookmarkManager:
                browser_test_helper_.profile()];
    ASSERT_TRUE(manager_);
    listController_ = [manager_ listController];
    ASSERT_TRUE(listController_);
    groupsController_ = [manager_ groupsController];
    ASSERT_TRUE(groupsController_);
  }

  void TearDown() {
    [manager_ close];
    [pasteboard_ releaseGlobally];
    CocoaTest::TearDown();
  }

  BookmarkItem* SelectBar() {
    BookmarkItem* bar = [manager_ bookmarkBarItem];
    EXPECT_TRUE(bar);
    [[manager_ groupsController] setSelectedItem:bar];
    return bar;
  }

  BookmarkItem* AddToBar(NSString*title, NSString* urlStr) {
    BookmarkItem* bar = [manager_ bookmarkBarItem];
    return [bar addBookmarkWithTitle:title
                                 URL:urlStr
                             atIndex:[bar numberOfChildren]];
  }

  BookmarkItem* AddFolderToBar(NSString* title) {
    BookmarkItem* bar = [manager_ bookmarkBarItem];
    return [bar addFolderWithTitle:title
                           atIndex:[bar numberOfChildren]];
  }

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* manager_;
  BookmarkTreeController* listController_;
  BookmarkTreeController* groupsController_;
  NSPasteboard* pasteboard_;
};

TEST_F(BookmarkTreeControllerTest, Model) {
  // Select nothing in the group list and check tree is empty:
  BookmarkTreeController* groupsController = [manager_ groupsController];
  [groupsController setSelectedItem:nil];
  EXPECT_EQ(nil, [listController_ group]);

  // Select the bookmarks bar and check that it's shown in the tree:
  BookmarkItem* bar = SelectBar();
  EXPECT_EQ(bar, [listController_ group]);
}

TEST_F(BookmarkTreeControllerTest, Selection) {
  SelectBar();
  BookmarkItem* test1 = AddToBar(@"Test 1", @"http://example.com/test1");
  BookmarkItem* test2 = AddToBar(@"Test 2", @"http://example.com/test2");
  BookmarkItem* test3 = AddToBar(@"Test 3", @"http://example.com/test3");
  EXPECT_EQ(0U, [[listController_ selectedItems] count]);

  NSArray* sel = [NSArray arrayWithObject:test2];
  [listController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[listController_ selectedItems]]);
  sel = [NSArray arrayWithObjects:test1, test3, nil];
  [listController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[listController_ selectedItems]]);
  sel = [NSArray arrayWithObjects:test1, test2, test3, nil];
  [listController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[listController_ selectedItems]]);
  sel = [NSArray array];
  [listController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[listController_ selectedItems]]);
}

TEST_F(BookmarkTreeControllerTest, NewFolder) {
  // Select Bookmark Bar in tree and create a new folder:
  BookmarkItem* bar = SelectBar();
  EXPECT_EQ(0U, [bar numberOfChildren]);
  BookmarkItem* parent = nil;
  NSUInteger index = 0;
  EXPECT_TRUE([groupsController_ getInsertionParent:&parent index:&index]);
  EXPECT_EQ(bar, parent);
  EXPECT_EQ(0U, index);
  [groupsController_ newFolder:nil];

  // Verify the new folder exists and is selected:
  ASSERT_EQ(1U, [bar numberOfChildren]);
  BookmarkItem *newFolder = [bar childAtIndex:0];
  EXPECT_EQ(newFolder, [groupsController_ selectedItem]);

  // Do New Folder again:
  EXPECT_TRUE([groupsController_ getInsertionParent:&parent index:&index]);
  EXPECT_EQ(bar, parent);
  EXPECT_EQ(0U, index);
  EXPECT_TRUE([listController_ getInsertionParent:&parent index:&index]);
  EXPECT_EQ(newFolder, parent);
  EXPECT_EQ(0U, index);
  [groupsController_ newFolder:nil];

  // Verify the new folder exists and is selected:
  ASSERT_EQ(2U, [bar numberOfChildren]);
  newFolder = [bar childAtIndex:0];
  EXPECT_EQ(newFolder, [groupsController_ selectedItem]);

  // Verify it's possible to add to Other Bookmarks:
  [groupsController_ setSelectedItem:[manager_ otherBookmarksItem]];
  EXPECT_TRUE([groupsController_ canInsert]);
  EXPECT_TRUE([listController_ canInsert]);

  // Verify it's not possible to add to Recents:
  [groupsController_ setSelectedItem:[manager_ recentGroup]];
  EXPECT_FALSE([groupsController_ canInsert]);
  EXPECT_FALSE([listController_ canInsert]);
}

TEST_F(BookmarkTreeControllerTest, MoveItems) {
  NSOutlineView* outline = [groupsController_ outline];
  ASSERT_TRUE(outline);

  // Add three folders and another one we'll drop some into:
  BookmarkItem* bookmarkBar = SelectBar();
  BookmarkItem* test1 = AddFolderToBar(@"Test 1");
  BookmarkItem* test2 = AddFolderToBar(@"Test 2");
  BookmarkItem* folder = AddFolderToBar(@"Folder");
  BookmarkItem* test3 = AddFolderToBar(@"Test 3");
  EXPECT_TRUE([groupsController_ expandItem:bookmarkBar]);

  // Verify NSOutlineView hierarchy.
  EXPECT_EQ(0, [outline rowForItem:bookmarkBar]);
  EXPECT_EQ(1, [outline rowForItem:test1]);
  EXPECT_EQ(2, [outline rowForItem:test2]);
  EXPECT_EQ(3, [outline rowForItem:folder]);
  EXPECT_EQ(4, [outline rowForItem:test3]);
  EXPECT_EQ(bookmarkBar, [outline parentForItem:test1]);
  EXPECT_EQ(bookmarkBar, [outline parentForItem:test2]);
  EXPECT_EQ(bookmarkBar, [outline parentForItem:folder]);
  EXPECT_EQ(bookmarkBar, [outline parentForItem:test3]);

  // Check where dropped URLs would go:
  NSInteger dropIndex = 0;
  BookmarkItem* target = [groupsController_ itemForDropOnItem:folder
                                                proposedIndex:&dropIndex];
  EXPECT_EQ(folder, target);
  EXPECT_EQ(0, dropIndex);

  dropIndex = NSOutlineViewDropOnItemIndex;
  target = [groupsController_ itemForDropOnItem:folder
                                  proposedIndex:&dropIndex];
  EXPECT_EQ(folder, target);
  EXPECT_EQ(0, dropIndex);

  // Move the first and third item into the folder.
  [groupsController_ moveItems:[NSMutableArray arrayWithObjects:test1, test3, nil]
                      toFolder:folder
                       atIndex:0];

  // Verify bookmark model hierarchy.
  EXPECT_EQ(folder, [test1 parent]);
  EXPECT_EQ(folder, [test3 parent]);
  EXPECT_EQ(bookmarkBar, [folder parent]);
  EXPECT_EQ(bookmarkBar, [test2 parent]);

  // Verify NSOutlineView hierarchy. test1 and test3 should be in the folder.
  EXPECT_TRUE([groupsController_ expandItem:folder]);
  EXPECT_TRUE([outline isItemExpanded:folder]);
  EXPECT_EQ(bookmarkBar, [outline parentForItem:test2]);
  EXPECT_EQ(bookmarkBar, [outline parentForItem:folder]);
  EXPECT_EQ(folder, [outline parentForItem:test1]);
  EXPECT_EQ(folder, [outline parentForItem:test3]);

  // Verify the folder is selected.
  EXPECT_TRUE([[outline selectedRowIndexes]
    isEqual:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(2, 1)]]);
  NSArray* sel = [groupsController_ selectedItems];
  EXPECT_TRUE([sel isEqual:([NSArray arrayWithObject:folder])]);
}

TEST_F(BookmarkTreeControllerTest, MoveItemsFlat) {
  // Add three bookmarks and a folder.
  NSOutlineView* outline = [listController_ outline];
  SelectBar();
  BookmarkItem* test1 = AddToBar(@"Test 1", @"http://example.com/test1");
  BookmarkItem* test2 = AddToBar(@"Test 2", @"http://example.com/test2");
  BookmarkItem* folder = AddFolderToBar(@"Folder");
  BookmarkItem* test3 = AddToBar(@"Test 3", @"http://example.com/test3");

  BookmarkItem* group = [listController_ group];

  // Check where dropped URLs would go:
  NSInteger dropIndex = NSOutlineViewDropOnItemIndex;
  BookmarkItem* target = [listController_ itemForDropOnItem:test1
                                              proposedIndex:&dropIndex];
  EXPECT_EQ(nil, target);
  dropIndex = 0;
  target = [listController_ itemForDropOnItem:test1
                                proposedIndex:&dropIndex];
  EXPECT_EQ(group, target);
  EXPECT_EQ(1, dropIndex);
  dropIndex = 0;
  target = [listController_ itemForDropOnItem:folder
                                proposedIndex:&dropIndex];
  EXPECT_EQ(folder, target);
  EXPECT_EQ(0, dropIndex);
  dropIndex = NSOutlineViewDropOnItemIndex;
  target = [listController_ itemForDropOnItem:folder
                                proposedIndex:&dropIndex];
  EXPECT_EQ(folder, target);
  EXPECT_EQ(0, dropIndex);

  // Move the first and third item into the folder.
  [listController_ moveItems:[NSArray arrayWithObjects:test1, test3, nil]
                    toFolder:folder
                     atIndex:0];

  // Verify bookmark model hierarchy.
  EXPECT_EQ(folder, [test1 parent]);
  EXPECT_EQ(folder, [test3 parent]);
  EXPECT_EQ(group, [folder parent]);
  EXPECT_EQ(group, [test2 parent]);

  // Verify NSOutlineView hierarchy. test1 and test3 should be gone now.
  EXPECT_EQ(-1, [outline rowForItem:test1]);
  EXPECT_EQ(-1, [outline rowForItem:test3]);
  EXPECT_EQ(nil, [outline parentForItem:test2]);
  EXPECT_EQ(nil, [outline parentForItem:folder]);
}

TEST_F(BookmarkTreeControllerTest, CopyURLs) {
  SelectBar();
  AddToBar(@"Test 1", @"http://example.com/test1");
  AddToBar(@"Test 2", @"http://example.com/test2");
  AddToBar(@"Test 3", @"http://example.com/test3");
  [[listController_ outline] selectAll:listController_];

  ASSERT_TRUE([listController_ copyToPasteboard:pasteboard_]);

  NSArray* contents = [pasteboard_ propertyListForType:
                            WebURLsWithTitlesPboardType];
  ASSERT_TRUE([contents isKindOfClass:[NSArray class]]);
  NSArray* urlStrings = [contents objectAtIndex:0];
  EXPECT_TRUE([urlStrings isKindOfClass:[NSArray class]]);
  NSArray* expectedURLStrings = [NSArray arrayWithObjects:
      @"http://example.com/test1",
      @"http://example.com/test2",
      @"http://example.com/test3", nil];
  EXPECT_TRUE([urlStrings isEqual:expectedURLStrings]);
  NSArray* titles = [contents objectAtIndex:1];
  EXPECT_TRUE([titles isKindOfClass:[NSArray class]]);
  NSArray* expectedTitles = [NSArray arrayWithObjects:
      @"Test 1",
      @"Test 2",
      @"Test 3", nil];
  EXPECT_TRUE([titles isEqual:expectedTitles]);

  NSString* str = [pasteboard_ stringForType:NSStringPboardType];
  EXPECT_TRUE([str isEqual:
      @"http://example.com/test1\n"
      "http://example.com/test2\n"
      "http://example.com/test3"]);

  EXPECT_FALSE([pasteboard_ dataForType:NSURLPboardType]);
}

TEST_F(BookmarkTreeControllerTest, PasteSingleURL) {
  [pasteboard_ declareTypes:[NSArray arrayWithObjects:
                                   NSURLPboardType, @"public.url-name", nil]
                      owner:nil];
  [[NSURL URLWithString:@"http://google.com/"] writeToPasteboard:pasteboard_];
  [pasteboard_ setString:@"Gooooogle" forType:@"public.url-name"];

  SelectBar();
  AddToBar(@"Test 1", @"http://example.com/test1");
  AddToBar(@"Test 2", @"http://example.com/test2");
  AddToBar(@"Test 3", @"http://example.com/test3");

  ASSERT_TRUE([listController_ pasteFromPasteboard:pasteboard_]);
  EXPECT_EQ(4, [[listController_ outline] numberOfRows]);
  EXPECT_EQ(3, [[listController_ outline] selectedRow]);
  NSArray* sel = [listController_ selectedItems];
  ASSERT_EQ(1U, [sel count]);
  BookmarkItem* item = [sel objectAtIndex:0];
  EXPECT_TRUE([@"http://google.com/" isEqual:[item URLString]]);
  EXPECT_TRUE([@"Gooooogle" isEqual:[item title]]);
}

TEST_F(BookmarkTreeControllerTest, PasteMultipleURLs) {
  [pasteboard_ declareTypes:[NSArray arrayWithObjects:
                                  WebURLsWithTitlesPboardType,
                                  NSURLPboardType, nil]
                      owner:nil];
  NSMutableArray* urls = [NSArray arrayWithObjects:
      @"http://google.com/", @"http://chromium.org/", nil];
  NSMutableArray* titles = [NSArray arrayWithObjects:
      @"Gooooogle", @"Chrooooomium", nil];
  [pasteboard_ setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
                       forType:WebURLsWithTitlesPboardType];
  [[NSURL URLWithString:@"http://example.com"] writeToPasteboard:pasteboard_];

  SelectBar();
  AddToBar(@"Test 1", @"http://example.com/test1");
  BookmarkItem* test2 = AddToBar(@"Test 2", @"http://example.com/test2");
  AddToBar(@"Test 3", @"http://example.com/test3");
  [listController_ setSelectedItems:[NSArray arrayWithObject:test2]];

  EXPECT_TRUE([listController_ readPropertyListFromPasteboard:pasteboard_]);
  ASSERT_TRUE([listController_ pasteFromPasteboard:pasteboard_]);
  EXPECT_EQ(5, [[listController_ outline] numberOfRows]);
  EXPECT_TRUE([[[listController_ outline] selectedRowIndexes]
      isEqual:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(1, 2)]]);
  NSArray* sel = [listController_ selectedItems];
  ASSERT_EQ(2U, [sel count]);

  BookmarkItem* item = [sel objectAtIndex:0];
  EXPECT_TRUE([@"http://google.com/" isEqual:[item URLString]]);
  EXPECT_TRUE([@"Gooooogle" isEqual:[item title]]);
  item = [sel objectAtIndex:1];
  EXPECT_TRUE([@"http://chromium.org/" isEqual:[item URLString]]);
  EXPECT_TRUE([@"Chrooooomium" isEqual:[item title]]);
}

}  // namespace
