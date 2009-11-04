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

class BookmarkEditorControllerTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  const BookmarkNode* default_node_;
  const BookmarkNode* default_parent_;
  const char* default_name_;
  std::wstring default_title_;
  BookmarkEditorController* default_controller_;

  virtual void SetUp() {
    CocoaTest::SetUp();
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    default_parent_ = model->GetBookmarkBarNode();
    default_name_ = "http://www.zim-bop-a-dee.com/";
    default_title_ = L"ooh title";
    const BookmarkNode* default_node = model->AddURL(default_parent_, 0,
                                                     default_title_,
                                                     GURL(default_name_));
    default_controller_ = [[BookmarkEditorController alloc]
                              initWithParentWindow:test_window()
                                           profile:helper_.profile()
                                            parent:default_parent_
                                              node:default_node
                                     configuration:BookmarkEditor::NO_TREE
                                           handler:nil];
    [default_controller_ window];  // Forces a nib load
  }

  virtual void TearDown() {
    [default_controller_ close];
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorControllerTest, NoNodeNoTree) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* node = NULL;

  BookmarkEditorController* controller =
      [[BookmarkEditorController alloc]
          initWithParentWindow:test_window()
                       profile:helper_.profile()
                        parent:parent
                          node:node
                 configuration:BookmarkEditor::NO_TREE
                       handler:nil];

  EXPECT_NE((NSWindow*)nil, [controller window]);  // Forces a nib load
  EXPECT_EQ(@"", [controller displayName]);
  EXPECT_EQ(@"", [controller displayURL]);
  EXPECT_FALSE([controller okButtonEnabled]);
  [controller close];
}

TEST_F(BookmarkEditorControllerTest, YesNodeShowTree) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const char* url_name = "http://www.zim-bop-a-dee.com/";
  const BookmarkNode* node = model->AddURL(parent, 0, default_title_,
                                           GURL(url_name));

  BookmarkEditorController* controller =
    [[BookmarkEditorController alloc]
        initWithParentWindow:test_window()
                     profile:helper_.profile()
                      parent:parent
                        node:node
               configuration:BookmarkEditor::SHOW_TREE
                     handler:nil];

  EXPECT_NE((NSWindow*)nil, [controller window]);  // Forces a nib load
  EXPECT_TRUE([base::SysWideToNSString(default_title_)
                isEqual:[controller displayName]]);
  EXPECT_TRUE([[NSString stringWithCString:url_name
                                  encoding:NSUTF8StringEncoding]
                isEqual:[controller displayURL]]);
  [controller close];
}

TEST_F(BookmarkEditorControllerTest, NoEdit) {
  [default_controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_EQ(child->GetTitle(), default_title_);
  EXPECT_EQ(child->GetURL(), GURL(default_name_));
}

TEST_F(BookmarkEditorControllerTest, EditTitle) {
  [default_controller_ setDisplayName:@"whamma jamma bamma"];
  [default_controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_EQ(child->GetTitle(), L"whamma jamma bamma");
  EXPECT_EQ(child->GetURL(), GURL(default_name_));
}

TEST_F(BookmarkEditorControllerTest, EditURL) {
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  [default_controller_ setDisplayURL:@"http://yellow-sneakers.com/"];
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  [default_controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_EQ(child->GetTitle(), default_title_);
  EXPECT_EQ(child->GetURL(), GURL("http://yellow-sneakers.com/"));
}

TEST_F(BookmarkEditorControllerTest, EditAndFixPrefix) {
  [default_controller_ setDisplayURL:@"x"];
  [default_controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_TRUE(child->GetURL().is_valid());
}

TEST_F(BookmarkEditorControllerTest, EditAndConfirmOKButton) {
  // Confirm OK button enabled/disabled as appropriate:
  // First test the URL.
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  [default_controller_ setDisplayURL:@""];
  EXPECT_FALSE([default_controller_ okButtonEnabled]);
  [default_controller_ setDisplayURL:@"http://www.cnn.com"];
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  // Then test the name.
  [default_controller_ setDisplayName:@""];
  EXPECT_FALSE([default_controller_ okButtonEnabled]);
  [default_controller_ setDisplayName:@"                   "];
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  // Then little mix of both.
  [default_controller_ setDisplayName:@"name"];
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  [default_controller_ setDisplayURL:@""];
  EXPECT_FALSE([default_controller_ okButtonEnabled]);
}

class BookmarkEditorControllerTreeTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;
  BookmarkEditorController* default_controller_;
  const BookmarkNode* group_a_;
  const BookmarkNode* group_b_;
  const BookmarkNode* group_bb_;
  const BookmarkNode* group_c_;
  const BookmarkNode* bookmark_bb_3_;

  BookmarkEditorControllerTreeTest() {
    // Set up a small bookmark hierarchy, which will look as follows:
    //    a      b      c    d
    //     a-0    b-0    c-0
    //     a-1     bb-0  c-1
    //     a-2     bb-1  c-2
    //             bb-2
    //             bb-3
    //             bb-4
    //            b-1
    //            b-2
    BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
    const BookmarkNode* root = model.GetBookmarkBarNode();
    group_a_ = model.AddGroup(root, 0, L"a");
    model.AddURL(group_a_, 0, L"a-0", GURL("http://a-0.com"));
    model.AddURL(group_a_, 1, L"a-1", GURL("http://a-1.com"));
    model.AddURL(group_a_, 2, L"a-2", GURL("http://a-2.com"));

    group_b_ = model.AddGroup(root, 1, L"b");
    model.AddURL(group_b_, 0, L"b-0", GURL("http://b-0.com"));
    group_bb_ = model.AddGroup(group_b_, 1, L"bb");
    model.AddURL(group_bb_, 0, L"bb-0", GURL("http://bb-0.com"));
    model.AddURL(group_bb_, 1, L"bb-1", GURL("http://bb-1.com"));
    model.AddURL(group_bb_, 2, L"bb-2", GURL("http://bb-2.com"));
    bookmark_bb_3_ =
      model.AddURL(group_bb_, 3, L"bb-3", GURL("http://bb-3.com"));
    model.AddURL(group_bb_, 4, L"bb-4", GURL("http://bb-4.com"));
    model.AddURL(group_b_, 2, L"b-2", GURL("http://b-2.com"));
    model.AddURL(group_b_, 3, L"b-2", GURL("http://b-3.com"));

    group_c_ = model.AddGroup(root, 2, L"c");
    model.AddURL(group_c_, 0, L"c-0", GURL("http://c-0.com"));
    model.AddURL(group_c_, 1, L"c-1", GURL("http://c-1.com"));
    model.AddURL(group_c_, 2, L"c-2", GURL("http://c-2.com"));
    model.AddURL(group_c_, 3, L"c-3", GURL("http://c-3.com"));

    model.AddURL(root, 3, L"d", GURL("http://d-0.com"));
  }

  virtual BookmarkEditorController* CreateController() {
    return [[BookmarkEditorController alloc]
               initWithParentWindow:test_window()
                            profile:helper_.profile()
                             parent:group_bb_
                               node:bookmark_bb_3_
                      configuration:BookmarkEditor::SHOW_TREE
                            handler:nil];
  }

  virtual void SetUp() {
    CocoaTest::SetUp();
    default_controller_ = CreateController();
    EXPECT_TRUE([default_controller_ window]);
  }

  virtual void TearDown() {
    [default_controller_ close];
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorControllerTreeTest, VerifyBookmarkTestModel) {
  BookmarkModel& model(*(helper_.profile()->GetBookmarkModel()));
  model.root_node();
  const BookmarkNode& root(*model.GetBookmarkBarNode());
  EXPECT_EQ(4, root.GetChildCount());
  const BookmarkNode* child = root.GetChild(0);
  EXPECT_EQ(3, child->GetChildCount());
  const BookmarkNode* subchild = child->GetChild(0);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(1);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->GetChildCount());

  child = root.GetChild(1);
  EXPECT_EQ(4, child->GetChildCount());
  subchild = child->GetChild(0);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(1);
  EXPECT_EQ(5, subchild->GetChildCount());
  const BookmarkNode* subsubchild = subchild->GetChild(0);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subsubchild = subchild->GetChild(1);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subsubchild = subchild->GetChild(2);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subsubchild = subchild->GetChild(3);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subsubchild = subchild->GetChild(4);
  EXPECT_EQ(0, subsubchild->GetChildCount());
  subchild = child->GetChild(2);
  EXPECT_EQ(0, subchild->GetChildCount());
  subchild = child->GetChild(3);
  EXPECT_EQ(0, subchild->GetChildCount());

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

  child = root.GetChild(3);
  EXPECT_EQ(0, child->GetChildCount());
}

TEST_F(BookmarkEditorControllerTreeTest, RenameBookmarkInPlace) {
  const BookmarkNode* oldParent = bookmark_bb_3_->GetParent();
  [default_controller_ setDisplayName:@"NEW NAME"];
  [default_controller_ ok:nil];
  const BookmarkNode* newParent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(newParent, oldParent);
  int childIndex = newParent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(3, childIndex);
}

TEST_F(BookmarkEditorControllerTreeTest, ChangeBookmarkURLInPlace) {
  const BookmarkNode* oldParent = bookmark_bb_3_->GetParent();
  [default_controller_ setDisplayURL:@"http://NEWURL.com"];
  [default_controller_ ok:nil];
  const BookmarkNode* newParent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(newParent, oldParent);
  int childIndex = newParent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(3, childIndex);
}

TEST_F(BookmarkEditorControllerTreeTest, ChangeBookmarkGroup) {
  [default_controller_ selectTestNodeInBrowser:group_c_];
  [default_controller_ ok:nil];
  const BookmarkNode* parent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(parent, group_c_);
  int childIndex = parent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(4, childIndex);
}

TEST_F(BookmarkEditorControllerTreeTest, ChangeNameAndBookmarkGroup) {
  [default_controller_ setDisplayName:@"NEW NAME"];
  [default_controller_ selectTestNodeInBrowser:group_c_];
  [default_controller_ ok:nil];
  const BookmarkNode* parent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(parent, group_c_);
  int childIndex = parent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(4, childIndex);
  EXPECT_EQ(bookmark_bb_3_->GetTitle(), L"NEW NAME");
}

TEST_F(BookmarkEditorControllerTreeTest, AddFolderWithGroupSelected) {
  [default_controller_ newFolder:nil];
  [default_controller_ cancel:nil];
  EXPECT_EQ(6, group_bb_->GetChildCount());
  const BookmarkNode* folderChild = group_bb_->GetChild(5);
  EXPECT_EQ(folderChild->GetTitle(), L"New folder");
}

class BookmarkEditorControllerTreeNoNodeTest :
    public BookmarkEditorControllerTreeTest {
 public:
  virtual BookmarkEditorController* CreateController() {
    return [[BookmarkEditorController alloc]
               initWithParentWindow:test_window()
                            profile:helper_.profile()
                             parent:group_bb_
                               node:nil
                      configuration:BookmarkEditor::SHOW_TREE
                            handler:nil];
  }

};

TEST_F(BookmarkEditorControllerTreeNoNodeTest, NewBookmarkNoNode) {
  [default_controller_ setDisplayName:@"NEW BOOKMARK"];
  [default_controller_ setDisplayURL:@"http://NEWURL.com"];
  [default_controller_ ok:nil];
  const BookmarkNode* new_node = group_bb_->GetChild(5);
  ASSERT_EQ(0, new_node->GetChildCount());
  EXPECT_EQ(new_node->GetTitle(), L"NEW BOOKMARK");
  EXPECT_EQ(new_node->GetURL(), GURL("http://NEWURL.com"));
}

class BookmarkEditorControllerTreeNoParentTest :
    public BookmarkEditorControllerTreeTest {
 public:
  virtual BookmarkEditorController* CreateController() {
    return [[BookmarkEditorController alloc]
               initWithParentWindow:test_window()
                            profile:helper_.profile()
                            parent:nil
                              node:nil
                     configuration:BookmarkEditor::SHOW_TREE
                           handler:nil];
      }
};

TEST_F(BookmarkEditorControllerTreeNoParentTest, AddFolderWithNoGroupSelected) {
  [default_controller_ newFolder:nil];
  [default_controller_ cancel:nil];
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* bookmarkBar = model->GetBookmarkBarNode();
  EXPECT_EQ(5, bookmarkBar->GetChildCount());
  const BookmarkNode* folderChild = bookmarkBar->GetChild(4);
  EXPECT_EQ(folderChild->GetTitle(), L"New folder");
}

