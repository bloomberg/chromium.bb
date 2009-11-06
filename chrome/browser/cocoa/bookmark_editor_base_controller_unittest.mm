// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BookmarkEditorBaseControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  BookmarkEditorBaseController* controller_;
  const BookmarkNode* group_a_;
  const BookmarkNode* group_b_;
  const BookmarkNode* group_b_0_;
  const BookmarkNode* group_b_3_;
  const BookmarkNode* group_c_;

  BookmarkEditorBaseControllerTest() {
    // Set up a small bookmark hierarchy, which will look as follows:
    //    a      b      c    d
    //     a-0    b-0    c-0
    //     a-1     b-00  c-1
    //     a-2    b-1    c-2
    //            b-2    c-3
    //            b-3
    //             b-30
    //             b-31
    //            b-4
    BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
    const BookmarkNode* root = model.GetBookmarkBarNode();
    group_a_ = model.AddGroup(root, 0, L"a");
    model.AddURL(group_a_, 0, L"a-0", GURL("http://a-0.com"));
    model.AddURL(group_a_, 1, L"a-1", GURL("http://a-1.com"));
    model.AddURL(group_a_, 2, L"a-2", GURL("http://a-2.com"));

    group_b_ = model.AddGroup(root, 1, L"b");
    group_b_0_ = model.AddGroup(group_b_, 0, L"b-0");
    model.AddURL(group_b_0_, 0, L"bb-0", GURL("http://bb-0.com"));
    model.AddURL(group_b_, 1, L"b-1", GURL("http://b-1.com"));
    model.AddURL(group_b_, 2, L"b-2", GURL("http://b-2.com"));
    group_b_3_ = model.AddGroup(group_b_, 3, L"b-3");
    model.AddURL(group_b_3_, 0, L"b-30", GURL("http://b-30.com"));
    model.AddURL(group_b_3_, 1, L"b-31", GURL("http://b-31.com"));
    model.AddURL(group_b_, 4, L"b-4", GURL("http://b-4.com"));

    group_c_ = model.AddGroup(root, 2, L"c");
    model.AddURL(group_c_, 0, L"c-0", GURL("http://c-0.com"));
    model.AddURL(group_c_, 1, L"c-1", GURL("http://c-1.com"));
    model.AddURL(group_c_, 2, L"c-2", GURL("http://c-2.com"));
    model.AddURL(group_c_, 3, L"c-3", GURL("http://c-3.com"));

    model.AddURL(root, 3, L"d", GURL("http://d-0.com"));
  }

  virtual BookmarkEditorBaseController* CreateController() {
    return [[BookmarkEditorBaseController alloc]
            initWithParentWindow:test_window()
                         nibName:@"BookmarkAllTabs"
                         profile:helper_.profile()
                          parent:group_b_0_
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

TEST_F(BookmarkEditorBaseControllerTest, VerifyBookmarkTestModel) {
  BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
  const BookmarkNode& root(*model.GetBookmarkBarNode());
  EXPECT_EQ(4, root.GetChildCount());
  // a
  const BookmarkNode* child = root.GetChild(0);
  EXPECT_EQ(3, child->GetChildCount());
  const BookmarkNode* subchild = child->GetChild(0);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->GetChildCount());
  // b
  child = root.GetChild(1);
  EXPECT_EQ(5, child->GetChildCount());
  subchild = child->GetChild(0);
  EXPECT_EQ(1, subchild->GetChildCount());
  const BookmarkNode* subsubchild = subchild->GetChild(0);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(3);
  EXPECT_EQ(2, subchild->GetChildCount());
  subsubchild = subchild->GetChild(0);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subsubchild = subchild->GetChild(1);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subchild = child->GetChild(4);
  EXPECT_EQ(0, subchild->GetChildCount());
  // c
  child = root.GetChild(2);
  EXPECT_EQ(4, child->GetChildCount());
  subchild = child->GetChild(0);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(3);
  EXPECT_EQ(0, subchild->GetChildCount());
  // d
  child = root.GetChild(3);
  EXPECT_EQ(0, child->GetChildCount());
}

TEST_F(BookmarkEditorBaseControllerTest, FolderChildForParent) {
  const BookmarkNode* child =
    [BookmarkEditorBaseController folderChildForParent:group_b_
                                       withFolderIndex:1];
  EXPECT_EQ(child, group_b_3_);
}

TEST_F(BookmarkEditorBaseControllerTest, IndexOfFolderChild) {
  int index = [BookmarkEditorBaseController indexOfFolderChild:group_b_3_];
  EXPECT_EQ(index, 1);
}

TEST_F(BookmarkEditorBaseControllerTest, NodeSelection) {
  [controller_ selectNodeInBrowser:group_b_3_];
  const BookmarkNode* node = [controller_ selectedNode];
  EXPECT_EQ(node, group_b_3_);
}
