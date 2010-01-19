// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

::testing::AssertionResult isEqual(id expected, id actual) {
  if(expected == actual || [expected isEqual:actual])
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure()
        << "Actual value: " << base::SysNSStringToWide([actual description]);
}

class BookmarkItemTest : public CocoaTest {
 public:
  void SetUp() {
    CocoaTest::SetUp();
    manager_ = [BookmarkManagerController showBookmarkManager:
                   browser_test_helper_.profile()];
    ASSERT_TRUE(manager_);
    model_ = [manager_ bookmarkModel];
    ASSERT_TRUE(model_);
    EXPECT_EQ(browser_test_helper_.profile()->GetBookmarkModel(), model_);
    barNode_ = model_->GetBookmarkBarNode();
    ASSERT_TRUE(barNode_);
    bar_ = [manager_ itemFromNode:barNode_];
    ASSERT_TRUE(bar_);
  }

  void AddTestBookmarks() {
    model_->AddURL(barNode_, 0, L"bookmark0", GURL("http://example.com/0"));
    model_->AddURL(barNode_, 1, L"bookmark1", GURL("http://example.com/1"));
    model_->AddURL(barNode_, 2, L"bookmark3", GURL("http://example.com/3"));
    const BookmarkNode* folder = model_->AddGroup(barNode_, 2, L"group");
    ASSERT_TRUE(folder);
    model_->AddURL(folder, 0, L"nested0", GURL("http://example.com/n0"));
    model_->AddURL(folder, 1, L"nested1", GURL("http://example.com/n1"));
  }

  void TearDown() {
    [manager_ close];
    CocoaTest::TearDown();
  }

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* manager_;
  BookmarkModel* model_;
  const BookmarkNode* barNode_;
  const BookmarkItem* bar_;
};

TEST_F(BookmarkItemTest, RootItems) {
  // Check the bookmarks-bar item:
  EXPECT_EQ(barNode_, [bar_ node]);
  EXPECT_EQ(bar_, [manager_ itemFromNode:barNode_]);
  EXPECT_EQ(bar_, [manager_ bookmarkBarItem]);
  EXPECT_TRUE([bar_ isFixed]);
  EXPECT_FALSE([bar_ isFake]);
  EXPECT_FALSE([bar_ parent]);
  EXPECT_TRUE([bar_ isFolder]);
  EXPECT_EQ(0U, [bar_ numberOfChildren]);

  // Check the 'others' item:
  const BookmarkNode* otherNode = model_->other_node();
  ASSERT_TRUE(otherNode);
  EXPECT_NE(barNode_, otherNode);
  BookmarkItem* other = [manager_ itemFromNode:otherNode];
  ASSERT_TRUE(other);
  EXPECT_EQ(otherNode, [other node]);
  EXPECT_EQ(other, [manager_ itemFromNode:otherNode]);
  EXPECT_EQ(other, [manager_ otherBookmarksItem]);
  EXPECT_TRUE([other isFixed]);
  EXPECT_FALSE([other isFake]);
  EXPECT_FALSE([other parent]);
  EXPECT_TRUE([other isFolder]);
  EXPECT_EQ(0U, [other numberOfChildren]);

  EXPECT_NE(bar_, other);
  EXPECT_FALSE(isEqual(bar_, other));
}

TEST_F(BookmarkItemTest, FakeItems) {
  NSImage* icon = [NSImage imageNamed:@"NSApplicationIcon"];
  EXPECT_TRUE(icon);
  FakeBookmarkItem* fake = [[FakeBookmarkItem alloc] initWithTitle:@"Fakir"
                                                              icon:icon
                                                           manager:manager_];
  [fake setChildren:nil];
  [fake autorelease];
  EXPECT_TRUE(isEqual(@"Fakir", [fake title]));
  EXPECT_EQ(icon, [fake icon]);
  EXPECT_TRUE([fake isFake]);
  EXPECT_TRUE([fake isFixed]);
  EXPECT_FALSE([fake parent]);
  EXPECT_FALSE([fake isFolder]);

  NSArray* children = [NSArray arrayWithObject:bar_];
  FakeBookmarkItem* bogus = [[FakeBookmarkItem alloc] initWithTitle:@"Bogus"
                                                               icon:nil
                                                            manager:manager_];
  [bogus setChildren:children];
  [bogus autorelease];
  EXPECT_EQ([BookmarkItem folderIcon], [bogus icon]);
  EXPECT_TRUE([bogus isFolder]);
  EXPECT_TRUE(isEqual(children, [bogus children]));
  ASSERT_EQ(1U, [bogus numberOfChildren]);
  EXPECT_EQ(bar_, [bogus childAtIndex:0]);

  BookmarkItem* otherItem = [manager_ itemFromNode:model_->other_node()];
  children = [NSArray arrayWithObjects:bar_, otherItem, nil];
  [bogus setChildren:children];
  EXPECT_TRUE([bogus isFolder]);
  EXPECT_TRUE(isEqual(children, [bogus children]));
  EXPECT_EQ(2U, [bogus numberOfChildren]);
  EXPECT_EQ(bar_, [bogus childAtIndex:0]);
  EXPECT_EQ(otherItem, [bogus childAtIndex:1]);
}

TEST_F(BookmarkItemTest, AddItems) {
  BookmarkItem* item1 = [bar_ addBookmarkWithTitle:@"item1"
                                               URL:@"http://example.com/item1"
                                           atIndex:0];
  EXPECT_TRUE(isEqual(@"item1", [item1 title]));
  EXPECT_TRUE(isEqual(@"http://example.com/item1", [item1 URLString]));
  EXPECT_TRUE([item1 icon]);
  EXPECT_FALSE([item1 isFixed]);
  EXPECT_FALSE([item1 isFolder]);
  EXPECT_EQ(bar_, [item1 parent]);
  EXPECT_EQ(item1, [bar_ childAtIndex:0]);

  BookmarkItem* folder = [bar_ addFolderWithTitle:@"folder"
                                          atIndex:1];
  EXPECT_TRUE(isEqual(@"folder", [folder title]));
  EXPECT_FALSE([folder URLString]);
  EXPECT_EQ([BookmarkItem folderIcon], [folder icon]);
  EXPECT_FALSE([folder isFixed]);
  EXPECT_TRUE([folder isFolder]);
  ASSERT_EQ(0U, [folder numberOfChildren]);
  EXPECT_EQ(bar_, [folder parent]);
  EXPECT_EQ(folder, [bar_ childAtIndex:1]);

  BookmarkItem* item2 = [folder addBookmarkWithTitle:@"item2"
                                                 URL:@"http://example.com/item2"
                                             atIndex:0];
  EXPECT_TRUE(isEqual(@"item2", [item2 title]));
  EXPECT_TRUE(isEqual(@"http://example.com/item2", [item2 URLString]));
  EXPECT_TRUE([item2 icon]);
  EXPECT_FALSE([item2 isFixed]);
  EXPECT_FALSE([item2 isFolder]);
  EXPECT_EQ(folder, [item2 parent]);
  EXPECT_EQ(item2, [folder childAtIndex:0]);
  EXPECT_EQ(0U, [folder indexOfChild:item2]);
}

TEST_F(BookmarkItemTest, URLEditing) {
  BookmarkItem* item = [bar_ addBookmarkWithTitle:@"URL"
                                              URL:@"http://www.google.com/"
                                          atIndex:0];
  EXPECT_TRUE(item);
  EXPECT_TRUE([@"http://www.google.com/" isEqual:[item URLString]]);
  [item setURLString: @"http://www.google.com/chrome"];
  EXPECT_TRUE([@"http://www.google.com/chrome" isEqual:[item URLString]]);
}

TEST_F(BookmarkItemTest, URLConversion) {
  BookmarkItem* item = [bar_ addBookmarkWithTitle:@"fixable URL"
                                              URL:@"www.google.com"
                                          atIndex:0];
  EXPECT_TRUE(item);
  EXPECT_TRUE([@"http://www.google.com/" isEqual:[item URLString]]);

  item = [bar_ addBookmarkWithTitle:@"bad URL"
                                URL:@"!$%@djd ^%QQQ"
                            atIndex:0];
  EXPECT_FALSE(item);
}

TEST_F(BookmarkItemTest, Hierarchy) {
  AddTestBookmarks();
  ASSERT_EQ(4U, [bar_ numberOfChildren]);
  NSString* barTitle = [bar_ title];

  // Check the bookmarks added to the bar.
  BookmarkItem* item = [bar_ childAtIndex:0];
  EXPECT_TRUE(isEqual(@"bookmark0", [item title]));
  EXPECT_TRUE(isEqual(@"http://example.com/0", [item URLString]));
  EXPECT_FALSE([item isFixed]);
  EXPECT_FALSE([item isFake]);
  EXPECT_EQ(bar_, [item parent]);
  EXPECT_FALSE([item isFolder]);
  EXPECT_TRUE(isEqual(barTitle, [item folderPath]));

  item = [bar_ childAtIndex:1];
  EXPECT_TRUE(isEqual(@"bookmark1", [item title]));
  EXPECT_TRUE(isEqual(@"http://example.com/1", [item URLString]));
  EXPECT_FALSE([item isFixed]);
  EXPECT_FALSE([item isFake]);
  EXPECT_EQ(bar_, [item parent]);
  EXPECT_FALSE([item isFolder]);

  item = [bar_ childAtIndex:3];
  EXPECT_TRUE(isEqual(@"bookmark3", [item title]));
  EXPECT_TRUE(isEqual(@"http://example.com/3", [item URLString]));
  EXPECT_FALSE([item isFixed]);
  EXPECT_FALSE([item isFake]);
  EXPECT_EQ(bar_, [item parent]);
  EXPECT_FALSE([item isFolder]);

  // Check the subfolder.
  BookmarkItem* folder = [bar_ childAtIndex:2];
  EXPECT_TRUE(isEqual(@"group", [folder title]));
  EXPECT_EQ(0U, [[folder URLString] length]);
  EXPECT_FALSE([item isFixed]);
  EXPECT_FALSE([folder isFake]);
  EXPECT_EQ(bar_, [folder parent]);
  EXPECT_TRUE([folder isFolder]);
  ASSERT_EQ(2U, [folder numberOfChildren]);
  EXPECT_TRUE(isEqual(barTitle, [folder folderPath]));

  // Check items in subfolder.
  item = [folder childAtIndex:0];
  EXPECT_TRUE(isEqual(@"nested0", [item title]));
  EXPECT_TRUE(isEqual(@"http://example.com/n0", [item URLString]));
  EXPECT_FALSE([item isFixed]);
  EXPECT_FALSE([item isFake]);
  EXPECT_EQ(folder, [item parent]);
  EXPECT_FALSE([item isFolder]);
  EXPECT_TRUE(isEqual([barTitle stringByAppendingString:@"/group"],
                      [item folderPath]));

  item = [folder childAtIndex:1];
  EXPECT_TRUE(isEqual(@"nested1", [item title]));
  EXPECT_TRUE(isEqual(@"http://example.com/n1", [item URLString]));
  EXPECT_FALSE([item isFixed]);
  EXPECT_FALSE([item isFake]);
  EXPECT_EQ(folder, [item parent]);
  EXPECT_FALSE([item isFolder]);
}

}  // namespace
