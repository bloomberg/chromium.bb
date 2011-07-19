// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_all_tabs_controller.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface BookmarkAllTabsControllerOverride : BookmarkAllTabsController
@end

@implementation BookmarkAllTabsControllerOverride

- (void)UpdateActiveTabPairs {
  ActiveTabsNameURLPairVector* activeTabPairsVector =
      [self activeTabPairsVector];
  activeTabPairsVector->clear();
  activeTabPairsVector->push_back(
      ActiveTabNameURLPair(ASCIIToUTF16("at-0"), GURL("http://at-0.com")));
  activeTabPairsVector->push_back(
      ActiveTabNameURLPair(ASCIIToUTF16("at-1"), GURL("http://at-1.com")));
  activeTabPairsVector->push_back(
      ActiveTabNameURLPair(ASCIIToUTF16("at-2"), GURL("http://at-2.com")));
}

@end

class BookmarkAllTabsControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  const BookmarkNode* parent_node_;
  BookmarkAllTabsControllerOverride* controller_;
  const BookmarkNode* folder_a_;

  BookmarkAllTabsControllerTest() {
    BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
    const BookmarkNode* root = model.GetBookmarkBarNode();
    folder_a_ = model.AddFolder(root, 0, ASCIIToUTF16("a"));
    model.AddURL(folder_a_, 0, ASCIIToUTF16("a-0"), GURL("http://a-0.com"));
    model.AddURL(folder_a_, 1, ASCIIToUTF16("a-1"), GURL("http://a-1.com"));
    model.AddURL(folder_a_, 2, ASCIIToUTF16("a-2"), GURL("http://a-2.com"));
  }

  virtual BookmarkAllTabsControllerOverride* CreateController() {
    return [[BookmarkAllTabsControllerOverride alloc]
            initWithParentWindow:test_window()
                         profile:helper_.profile()
                          parent:folder_a_
                   configuration:BookmarkEditor::SHOW_TREE];
  }

  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_ = CreateController();
    [controller_ runAsModalSheet];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkAllTabsControllerTest, BookmarkAllTabs) {
  // OK button should always be enabled.
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ selectTestNodeInBrowser:folder_a_];
  [controller_ setDisplayName:@"ALL MY TABS"];
  [controller_ ok:nil];
  EXPECT_EQ(4, folder_a_->child_count());
  const BookmarkNode* folderChild = folder_a_->GetChild(3);
  EXPECT_EQ(folderChild->GetTitle(), ASCIIToUTF16("ALL MY TABS"));
  EXPECT_EQ(3, folderChild->child_count());
}
