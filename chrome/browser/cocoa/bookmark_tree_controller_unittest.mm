// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_groups_controller.h"
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
    treeController_ = [manager_ treeController];
    ASSERT_TRUE(treeController_);
  }

  void TearDown() {
    [manager_ close];
    [pasteboard_ releaseGlobally];
    CocoaTest::TearDown();
  }

  id SelectBar() {
    BookmarkModel* model = [manager_ bookmarkModel];
    id barItem = [manager_ itemFromNode:model->GetBookmarkBarNode()];
    [[manager_ groupsController] setSelectedGroup:barItem];
    return barItem;
  }

  id AddToBar(const std::wstring& title, const std::string& urlStr) {
    BookmarkModel* model = [manager_ bookmarkModel];
    const BookmarkNode* bar = model->GetBookmarkBarNode();
    const BookmarkNode* node;
    node = model->AddURL(bar, bar->GetChildCount(), title, GURL(urlStr));
    return [manager_ itemFromNode:node];
  }

  id AddFolderToBar(const std::wstring& title) {
    BookmarkModel* model = [manager_ bookmarkModel];
    const BookmarkNode* bar = model->GetBookmarkBarNode();
    const BookmarkNode* node;
    node = model->AddGroup(bar, bar->GetChildCount(), title);
    return [manager_ itemFromNode:node];
  }

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* manager_;
  BookmarkTreeController* treeController_;
  NSPasteboard* pasteboard_;
};

TEST_F(BookmarkTreeControllerTest, Model) {
  // Select nothing in the group list and check tree is empty:
  BookmarkGroupsController* groupsController = [manager_ groupsController];
  [groupsController setSelectedGroup:nil];
  EXPECT_EQ(nil, [treeController_ group]);

  // Select the bookmarks bar and check that it's shown in the tree:
  id barItem = SelectBar();
  EXPECT_EQ(barItem, [treeController_ group]);

  // Check the outline-item mapping methods:
  const BookmarkNode* barNode = [manager_ nodeFromItem:barItem];
  EXPECT_EQ(nil, [treeController_ itemFromNode:barNode]);
  EXPECT_EQ(barNode, [treeController_ nodeFromItem:nil]);
}

TEST_F(BookmarkTreeControllerTest, Selection) {
  SelectBar();
  id test1 = AddToBar(L"Test 1", "http://example.com/test1");
  id test2 = AddToBar(L"Test 2", "http://example.com/test2");
  id test3 = AddToBar(L"Test 3", "http://example.com/test3");
  EXPECT_EQ(0U, [[treeController_ selectedItems] count]);

  NSArray* sel = [NSArray arrayWithObject:test2];
  [treeController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[treeController_ selectedItems]]);
  sel = [NSArray arrayWithObjects:test1, test3, nil];
  [treeController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[treeController_ selectedItems]]);
  sel = [NSArray arrayWithObjects:test1, test2, test3, nil];
  [treeController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[treeController_ selectedItems]]);
  sel = [NSArray array];
  [treeController_ setSelectedItems:sel];
  EXPECT_TRUE([sel isEqual:[treeController_ selectedItems]]);
}

TEST_F(BookmarkTreeControllerTest, MoveNodes) {
  // Add three bookmarks and a folder.
  NSOutlineView* outline = [treeController_ outline];
  SelectBar();
  id test1 = AddToBar(L"Test 1", "http://example.com/test1");
  id test2 = AddToBar(L"Test 2", "http://example.com/test2");
  id folder = AddFolderToBar(L"Folder");
  id test3 = AddToBar(L"Test 3", "http://example.com/test3");
  [outline expandItem:folder];

  const BookmarkNode* groupNode = [treeController_ nodeFromItem:nil];
  const BookmarkNode* node1 = [manager_ nodeFromItem:test1];
  const BookmarkNode* node2 = [manager_ nodeFromItem:test2];
  const BookmarkNode* folderNode = [manager_ nodeFromItem:folder];
  const BookmarkNode* node3 = [manager_ nodeFromItem:test3];

  // Check where dropped URLs would go:
  NSInteger dropIndex = NSOutlineViewDropOnItemIndex;
  const BookmarkNode* target = [treeController_ nodeForDropOnItem:test1
                                                    proposedIndex:&dropIndex];
  EXPECT_EQ(NULL, target);
  dropIndex = 0;
  target = [treeController_ nodeForDropOnItem:test1
                                proposedIndex:&dropIndex];
  EXPECT_EQ(groupNode, target);
  EXPECT_EQ(1, dropIndex);
  dropIndex = 0;
  target = [treeController_ nodeForDropOnItem:folder
                                proposedIndex:&dropIndex];
  EXPECT_EQ(folderNode, target);
  EXPECT_EQ(0, dropIndex);
  dropIndex = NSOutlineViewDropOnItemIndex;
  target = [treeController_ nodeForDropOnItem:folder
                                proposedIndex:&dropIndex];
  EXPECT_EQ(folderNode, target);
  EXPECT_EQ(0, dropIndex);

  // Move the first and third item into the folder.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node1);
  nodes.push_back(node3);
  [treeController_ moveNodes:nodes toFolder:folderNode atIndex:0];

  // Verify bookmark model hierarchy.
  EXPECT_EQ(folderNode, node1->GetParent());
  EXPECT_EQ(folderNode, node3->GetParent());
  EXPECT_EQ(groupNode, folderNode->GetParent());
  EXPECT_EQ(groupNode, node2->GetParent());

  // Verify NSOutlineView hierarchy.
  EXPECT_EQ(folder, [outline parentForItem:test1]);
  EXPECT_EQ(folder, [outline parentForItem:test3]);
  EXPECT_EQ(nil, [outline parentForItem:test2]);
  EXPECT_EQ(nil, [outline parentForItem:folder]);

  // Verify the moved nodes are selected.
  EXPECT_TRUE([[outline selectedRowIndexes]
    isEqual:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(2, 2)]]);
  NSArray* sel = [treeController_ selectedItems];
  EXPECT_TRUE([sel isEqual:([NSArray arrayWithObjects:test1, test3, nil])]);
}

TEST_F(BookmarkTreeControllerTest, CopyURLs) {
  SelectBar();
  AddToBar(L"Test 1", "http://example.com/test1");
  AddToBar(L"Test 2", "http://example.com/test2");
  AddToBar(L"Test 3", "http://example.com/test3");
  [[treeController_ outline] selectAll:treeController_];

  ASSERT_TRUE([treeController_ copyToPasteboard:pasteboard_]);

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
  [[NSURL URLWithString:@"http://google.com"] writeToPasteboard:pasteboard_];
  [pasteboard_ setString:@"Gooooogle" forType:@"public.url-name"];

  SelectBar();
  AddToBar(L"Test 1", "http://example.com/test1");
  AddToBar(L"Test 2", "http://example.com/test2");
  AddToBar(L"Test 3", "http://example.com/test3");

  ASSERT_TRUE([treeController_ pasteFromPasteboard:pasteboard_]);
  EXPECT_EQ(4, [[treeController_ outline] numberOfRows]);
  EXPECT_EQ(3, [[treeController_ outline] selectedRow]);
  NSArray* sel = [treeController_ selectedItems];
  ASSERT_EQ(1U, [sel count]);
  const BookmarkNode* node = [manager_ nodeFromItem:[sel objectAtIndex:0]];
  ASSERT_TRUE(node);
  EXPECT_EQ(GURL("http://google.com"), node->GetURL());
  EXPECT_EQ(L"Gooooogle", node->GetTitle());
}

TEST_F(BookmarkTreeControllerTest, PasteMultipleURLs) {
  [pasteboard_ declareTypes:[NSArray arrayWithObjects:
                                  WebURLsWithTitlesPboardType,
                                  NSURLPboardType, nil]
                      owner:nil];
  NSMutableArray* urls = [NSArray arrayWithObjects:
      @"http://google.com", @"http://chromium.org", nil];
  NSMutableArray* titles = [NSArray arrayWithObjects:
      @"Gooooogle", @"Chrooooomium", nil];
  [pasteboard_ setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
                       forType:WebURLsWithTitlesPboardType];
  [[NSURL URLWithString:@"http://example.com"] writeToPasteboard:pasteboard_];

  SelectBar();
  AddToBar(L"Test 1", "http://example.com/test1");
  id test2 = AddToBar(L"Test 2", "http://example.com/test2");
  AddToBar(L"Test 3", "http://example.com/test3");
  [treeController_ setSelectedItems:[NSArray arrayWithObject:test2]];

  ASSERT_TRUE([treeController_ pasteFromPasteboard:pasteboard_]);
  EXPECT_EQ(5, [[treeController_ outline] numberOfRows]);
  EXPECT_TRUE([[[treeController_ outline] selectedRowIndexes]
      isEqual:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(2, 2)]]);
  NSArray* sel = [treeController_ selectedItems];
  ASSERT_EQ(2U, [sel count]);

  const BookmarkNode* node = [manager_ nodeFromItem:[sel objectAtIndex:0]];
  ASSERT_TRUE(node);
  EXPECT_EQ(GURL("http://google.com"), node->GetURL());
  EXPECT_EQ(L"Gooooogle", node->GetTitle());
  node = [manager_ nodeFromItem:[sel objectAtIndex:1]];
  ASSERT_TRUE(node);
  EXPECT_EQ(GURL("http://chromium.org"), node->GetURL());
  EXPECT_EQ(L"Chrooooomium", node->GetTitle());
}

}  // namespace
