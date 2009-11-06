// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_all_tabs_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
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
      ActiveTabNameURLPair(L"at-0", GURL("http://at-0.com")));
  activeTabPairsVector->push_back(
      ActiveTabNameURLPair(L"at-1", GURL("http://at-1.com")));
  activeTabPairsVector->push_back(
      ActiveTabNameURLPair(L"at-2", GURL("http://at-2.com")));
}

@end

class BookmarkAllTabsControllerTest : public CocoaTest {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  const BookmarkNode* parent_node_;
  BookmarkAllTabsControllerOverride* controller_;
  const BookmarkNode* group_a_;

  BookmarkAllTabsControllerTest() {
    BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
    const BookmarkNode* root = model.GetBookmarkBarNode();
    group_a_ = model.AddGroup(root, 0, L"a");
    model.AddURL(group_a_, 0, L"a-0", GURL("http://a-0.com"));
    model.AddURL(group_a_, 1, L"a-1", GURL("http://a-1.com"));
    model.AddURL(group_a_, 2, L"a-2", GURL("http://a-2.com"));
  }

  virtual BookmarkAllTabsControllerOverride* CreateController() {
    return [[BookmarkAllTabsControllerOverride alloc]
            initWithParentWindow:cocoa_helper_.window()
                         profile:helper_.profile()
                          parent:group_a_
                   configuration:BookmarkEditor::SHOW_TREE
                         handler:nil];
  }

  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_ = CreateController();
    EXPECT_TRUE([controller_ window]);
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkAllTabsControllerTest, BookmarkAllTabs) {
  // OK button should always be enabled.
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ setDisplayName:@"ALL MY TABS"];
  [controller_ ok:nil];
  EXPECT_EQ(4, group_a_->GetChildCount());
  const BookmarkNode* folderChild = group_a_->GetChild(3);
  EXPECT_EQ(folderChild->GetTitle(), L"ALL MY TABS");
  EXPECT_EQ(3, folderChild->GetChildCount());
}
