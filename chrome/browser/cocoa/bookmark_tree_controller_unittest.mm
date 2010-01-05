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

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* manager_;
  BookmarkTreeController* treeController_;
  NSPasteboard* pasteboard_;
};

TEST_F(BookmarkTreeControllerTest, Model) {
  // Select nothing in the group list and check tree is empty:
  BookmarkGroupsController* groupsController = [manager_ groupsController];
  [groupsController setSelectedGroup:nil];
  ASSERT_EQ(nil, [treeController_ group]);

  // Select the bookmarks bar and check that it's shown in the tree:
  id barItem = SelectBar();
  ASSERT_EQ(barItem, [treeController_ group]);
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
