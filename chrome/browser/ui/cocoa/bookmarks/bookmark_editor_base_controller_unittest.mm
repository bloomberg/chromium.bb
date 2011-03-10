// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_controller.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

class BookmarkEditorBaseControllerTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
  BookmarkEditorBaseController* controller_;  // weak
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
    BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
    const BookmarkNode* root = model.GetBookmarkBarNode();
    group_a_ = model.AddGroup(root, 0, ASCIIToUTF16("a"));
    model.AddURL(group_a_, 0, ASCIIToUTF16("a-0"), GURL("http://a-0.com"));
    model.AddURL(group_a_, 1, ASCIIToUTF16("a-1"), GURL("http://a-1.com"));
    model.AddURL(group_a_, 2, ASCIIToUTF16("a-2"), GURL("http://a-2.com"));

    group_b_ = model.AddGroup(root, 1, ASCIIToUTF16("b"));
    group_b_0_ = model.AddGroup(group_b_, 0, ASCIIToUTF16("b-0"));
    model.AddURL(group_b_0_, 0, ASCIIToUTF16("bb-0"), GURL("http://bb-0.com"));
    model.AddURL(group_b_, 1, ASCIIToUTF16("b-1"), GURL("http://b-1.com"));
    model.AddURL(group_b_, 2, ASCIIToUTF16("b-2"), GURL("http://b-2.com"));
    group_b_3_ = model.AddGroup(group_b_, 3, ASCIIToUTF16("b-3"));
    model.AddURL(group_b_3_, 0, ASCIIToUTF16("b-30"), GURL("http://b-30.com"));
    model.AddURL(group_b_3_, 1, ASCIIToUTF16("b-31"), GURL("http://b-31.com"));
    model.AddURL(group_b_, 4, ASCIIToUTF16("b-4"), GURL("http://b-4.com"));

    group_c_ = model.AddGroup(root, 2, ASCIIToUTF16("c"));
    model.AddURL(group_c_, 0, ASCIIToUTF16("c-0"), GURL("http://c-0.com"));
    model.AddURL(group_c_, 1, ASCIIToUTF16("c-1"), GURL("http://c-1.com"));
    model.AddURL(group_c_, 2, ASCIIToUTF16("c-2"), GURL("http://c-2.com"));
    model.AddURL(group_c_, 3, ASCIIToUTF16("c-3"), GURL("http://c-3.com"));

    model.AddURL(root, 3, ASCIIToUTF16("d"), GURL("http://d-0.com"));
  }

  virtual BookmarkEditorBaseController* CreateController() {
    return [[BookmarkEditorBaseController alloc]
            initWithParentWindow:test_window()
                         nibName:@"BookmarkAllTabs"
                         profile:browser_helper_.profile()
                          parent:group_b_0_
                   configuration:BookmarkEditor::SHOW_TREE];
  }

  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_ = CreateController();
    EXPECT_TRUE([controller_ window]);
    [controller_ runAsModalSheet];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorBaseControllerTest, VerifyBookmarkTestModel) {
  BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
  const BookmarkNode& root(*model.GetBookmarkBarNode());
  EXPECT_EQ(4, root.child_count());
  // a
  const BookmarkNode* child = root.GetChild(0);
  EXPECT_EQ(3, child->child_count());
  const BookmarkNode* subchild = child->GetChild(0);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->child_count());
  // b
  child = root.GetChild(1);
  EXPECT_EQ(5, child->child_count());
  subchild = child->GetChild(0);
  EXPECT_EQ(1, subchild->child_count());
  const BookmarkNode* subsubchild = subchild->GetChild(0);
  EXPECT_EQ(0, subsubchild->child_count());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(3);
  EXPECT_EQ(2, subchild->child_count());
  subsubchild = subchild->GetChild(0);
  EXPECT_EQ(0, subsubchild->child_count());
  subsubchild = subchild->GetChild(1);
  EXPECT_EQ(0, subsubchild->child_count());
  subchild = child->GetChild(4);
  EXPECT_EQ(0, subchild->child_count());
  // c
  child = root.GetChild(2);
  EXPECT_EQ(4, child->child_count());
  subchild = child->GetChild(0);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->child_count());
  subchild = child->GetChild(3);
  EXPECT_EQ(0, subchild->child_count());
  // d
  child = root.GetChild(3);
  EXPECT_EQ(0, child->child_count());
  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorBaseControllerTest, NodeSelection) {
  EXPECT_TRUE([controller_ folderTreeArray]);
  [controller_ selectTestNodeInBrowser:group_b_3_];
  const BookmarkNode* node = [controller_ selectedNode];
  EXPECT_EQ(node, group_b_3_);
  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorBaseControllerTest, CreateFolder) {
  EXPECT_EQ(2, group_b_3_->child_count());
  [controller_ selectTestNodeInBrowser:group_b_3_];
  NSString* expectedName =
      l10n_util::GetNSStringWithFixup(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME);
  [controller_ setDisplayName:expectedName];
  [controller_ newFolder:nil];
  NSArray* selectionPaths = [controller_ tableSelectionPaths];
  EXPECT_EQ(1U, [selectionPaths count]);
  NSIndexPath* selectionPath = [selectionPaths objectAtIndex:0];
  EXPECT_EQ(4U, [selectionPath length]);
  BookmarkFolderInfo* newFolderInfo = [controller_ selectedFolder];
  EXPECT_TRUE(newFolderInfo);
  NSString* newFolderName = [newFolderInfo folderName];
  EXPECT_NSEQ(expectedName, newFolderName);
  [controller_ createNewFolders];
  // Verify that the tab folder was added to the new folder.
  EXPECT_EQ(3, group_b_3_->child_count());
  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorBaseControllerTest, CreateTwoFolders) {
  BookmarkModel* model = browser_helper_.profile()->GetBookmarkModel();
  const BookmarkNode* bar = model->GetBookmarkBarNode();
  // Create 2 folders which are children of the bar.
  [controller_ selectTestNodeInBrowser:bar];
  [controller_ newFolder:nil];
  [controller_ selectTestNodeInBrowser:bar];
  [controller_ newFolder:nil];
  // If we do NOT crash on createNewFolders, success!
  // (e.g. http://crbug.com/47877 is fixed).
  [controller_ createNewFolders];
  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorBaseControllerTest, SelectedFolderDeleted) {
  BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
  [controller_ selectTestNodeInBrowser:group_b_3_];
  EXPECT_EQ(group_b_3_, [controller_ selectedNode]);

  // Delete the selected node, and verify it's no longer selected:
  model.Remove(group_b_, 3);
  EXPECT_NE(group_b_3_, [controller_ selectedNode]);

  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorBaseControllerTest, SelectedFoldersParentDeleted) {
  BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
  const BookmarkNode* root = model.GetBookmarkBarNode();
  [controller_ selectTestNodeInBrowser:group_b_3_];
  EXPECT_EQ(group_b_3_, [controller_ selectedNode]);

  // Delete the selected node's parent, and verify it's no longer selected:
  model.Remove(root, 1);
  EXPECT_NE(group_b_3_, [controller_ selectedNode]);

  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorBaseControllerTest, FolderAdded) {
  BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
  const BookmarkNode* root = model.GetBookmarkBarNode();

  // Add a group node to the model, and verify it can be selected in the tree:
  const BookmarkNode* group_added = model.AddGroup(root, 0,
                                                   ASCIIToUTF16("added"));
  [controller_ selectTestNodeInBrowser:group_added];
  EXPECT_EQ(group_added, [controller_ selectedNode]);

  [controller_ cancel:nil];
}


class BookmarkFolderInfoTest : public CocoaTest { };

TEST_F(BookmarkFolderInfoTest, Construction) {
  NSMutableArray* children = [NSMutableArray arrayWithObject:@"child"];
  // We just need a pointer, and any pointer will do.
  const BookmarkNode* fakeNode =
      reinterpret_cast<const BookmarkNode*>(&children);
  BookmarkFolderInfo* info =
    [BookmarkFolderInfo bookmarkFolderInfoWithFolderName:@"name"
                                              folderNode:fakeNode
                                                children:children];
  EXPECT_TRUE(info);
  EXPECT_EQ([info folderName], @"name");
  EXPECT_EQ([info children], children);
  EXPECT_EQ([info folderNode], fakeNode);
}
