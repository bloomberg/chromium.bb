// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BookmarkEditorControllerTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
  const BookmarkNode* default_node_;
  const BookmarkNode* default_parent_;
  const char* default_name_;
  std::wstring default_title_;
  BookmarkEditorController* controller_;

  virtual void SetUp() {
    CocoaTest::SetUp();
    BookmarkModel* model = browser_helper_.profile()->GetBookmarkModel();
    default_parent_ = model->GetBookmarkBarNode();
    default_name_ = "http://www.zim-bop-a-dee.com/";
    default_title_ = L"ooh title";
    const BookmarkNode* default_node = model->AddURL(default_parent_, 0,
                                                     default_title_,
                                                     GURL(default_name_));
    controller_ = [[BookmarkEditorController alloc]
                   initWithParentWindow:test_window()
                                profile:browser_helper_.profile()
                                 parent:default_parent_
                                   node:default_node
                          configuration:BookmarkEditor::NO_TREE
                                handler:nil];
    [controller_ runAsModalSheet];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorControllerTest, NoEdit) {
  [controller_ cancel:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_EQ(child->GetTitle(), default_title_);
  EXPECT_EQ(child->GetURL(), GURL(default_name_));
}

TEST_F(BookmarkEditorControllerTest, EditTitle) {
  [controller_ setDisplayName:@"whamma jamma bamma"];
  [controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_EQ(child->GetTitle(), L"whamma jamma bamma");
  EXPECT_EQ(child->GetURL(), GURL(default_name_));
}

TEST_F(BookmarkEditorControllerTest, EditURL) {
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ setDisplayURL:@"http://yellow-sneakers.com/"];
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_EQ(child->GetTitle(), default_title_);
  EXPECT_EQ(child->GetURL(), GURL("http://yellow-sneakers.com/"));
}

TEST_F(BookmarkEditorControllerTest, EditAndFixPrefix) {
  [controller_ setDisplayURL:@"x"];
  [controller_ ok:nil];
  ASSERT_EQ(default_parent_->GetChildCount(), 1);
  const BookmarkNode* child = default_parent_->GetChild(0);
  EXPECT_TRUE(child->GetURL().is_valid());
}

TEST_F(BookmarkEditorControllerTest, EditAndConfirmOKButton) {
  // Confirm OK button enabled/disabled as appropriate:
  // First test the URL.
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ setDisplayURL:@""];
  EXPECT_FALSE([controller_ okButtonEnabled]);
  [controller_ setDisplayURL:@"http://www.cnn.com"];
  EXPECT_TRUE([controller_ okButtonEnabled]);
  // Then test the name.
  [controller_ setDisplayName:@""];
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ setDisplayName:@"                   "];
  EXPECT_TRUE([controller_ okButtonEnabled]);
  // Then little mix of both.
  [controller_ setDisplayName:@"name"];
  EXPECT_TRUE([controller_ okButtonEnabled]);
  [controller_ setDisplayURL:@""];
  EXPECT_FALSE([controller_ okButtonEnabled]);
  [controller_ cancel:nil];
}

class BookmarkEditorControllerNoNodeTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
  BookmarkEditorController* controller_;

  virtual void SetUp() {
    CocoaTest::SetUp();
    BookmarkModel* model = browser_helper_.profile()->GetBookmarkModel();
    const BookmarkNode* parent = model->GetBookmarkBarNode();
    controller_ = [[BookmarkEditorController alloc]
                   initWithParentWindow:test_window()
                                profile:browser_helper_.profile()
                                 parent:parent
                                   node:NULL
                          configuration:BookmarkEditor::NO_TREE
                                handler:nil];

    [controller_ runAsModalSheet];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorControllerNoNodeTest, NoNodeNoTree) {
  EXPECT_EQ(@"", [controller_ displayName]);
  EXPECT_EQ(@"", [controller_ displayURL]);
  EXPECT_FALSE([controller_ okButtonEnabled]);
  [controller_ cancel:nil];
}

class BookmarkEditorControllerYesNodeTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
  std::wstring default_title_;
  const char* url_name_;
  BookmarkEditorController* controller_;

  virtual void SetUp() {
    CocoaTest::SetUp();
    BookmarkModel* model = browser_helper_.profile()->GetBookmarkModel();
    const BookmarkNode* parent = model->GetBookmarkBarNode();
    default_title_ = L"wooh title";
    url_name_ = "http://www.zoom-baby-doo-da.com/";
    const BookmarkNode* node = model->AddURL(parent, 0, default_title_,
                                             GURL(url_name_));
    controller_ = [[BookmarkEditorController alloc]
                   initWithParentWindow:test_window()
                                profile:browser_helper_.profile()
                                 parent:parent
                                   node:node
                          configuration:BookmarkEditor::NO_TREE
                                handler:nil];

    [controller_ runAsModalSheet];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorControllerYesNodeTest, YesNodeShowTree) {
  EXPECT_TRUE([base::SysWideToNSString(default_title_)
               isEqual:[controller_ displayName]]);
  EXPECT_TRUE([[NSString stringWithCString:url_name_
                                  encoding:NSUTF8StringEncoding]
               isEqual:[controller_ displayURL]]);
  [controller_ cancel:nil];
}

class BookmarkEditorControllerTreeTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
  BookmarkEditorController* controller_;
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
    BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
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
    model.AddURL(group_b_, 2, L"b-1", GURL("http://b-2.com"));
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
                            profile:browser_helper_.profile()
                             parent:group_bb_
                               node:bookmark_bb_3_
                      configuration:BookmarkEditor::SHOW_TREE
                            handler:nil];
  }

  virtual void SetUp() {
    controller_ = CreateController();
    [controller_ runAsModalSheet];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(BookmarkEditorControllerTreeTest, VerifyBookmarkTestModel) {
  BookmarkModel& model(*(browser_helper_.profile()->GetBookmarkModel()));
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
  [controller_ cancel:nil];
}

TEST_F(BookmarkEditorControllerTreeTest, RenameBookmarkInPlace) {
  const BookmarkNode* oldParent = bookmark_bb_3_->GetParent();
  [controller_ setDisplayName:@"NEW NAME"];
  [controller_ ok:nil];
  const BookmarkNode* newParent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(newParent, oldParent);
  int childIndex = newParent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(3, childIndex);
}

TEST_F(BookmarkEditorControllerTreeTest, ChangeBookmarkURLInPlace) {
  const BookmarkNode* oldParent = bookmark_bb_3_->GetParent();
  [controller_ setDisplayURL:@"http://NEWURL.com"];
  [controller_ ok:nil];
  const BookmarkNode* newParent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(newParent, oldParent);
  int childIndex = newParent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(3, childIndex);
}

TEST_F(BookmarkEditorControllerTreeTest, ChangeBookmarkGroup) {
  [controller_ selectTestNodeInBrowser:group_c_];
  [controller_ ok:nil];
  const BookmarkNode* parent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(parent, group_c_);
  int childIndex = parent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(4, childIndex);
}

TEST_F(BookmarkEditorControllerTreeTest, ChangeNameAndBookmarkGroup) {
  [controller_ setDisplayName:@"NEW NAME"];
  [controller_ selectTestNodeInBrowser:group_c_];
  [controller_ ok:nil];
  const BookmarkNode* parent = bookmark_bb_3_->GetParent();
  ASSERT_EQ(parent, group_c_);
  int childIndex = parent->IndexOfChild(bookmark_bb_3_);
  ASSERT_EQ(4, childIndex);
  EXPECT_EQ(bookmark_bb_3_->GetTitle(), L"NEW NAME");
}

TEST_F(BookmarkEditorControllerTreeTest, AddFolderWithGroupSelected) {
  // Folders are NOT added unless the OK button is pressed.
  [controller_ newFolder:nil];
  [controller_ cancel:nil];
  EXPECT_EQ(5, group_bb_->GetChildCount());
}

class BookmarkEditorControllerTreeNoNodeTest :
    public BookmarkEditorControllerTreeTest {
 public:
  virtual BookmarkEditorController* CreateController() {
    return [[BookmarkEditorController alloc]
               initWithParentWindow:test_window()
                            profile:browser_helper_.profile()
                             parent:group_bb_
                               node:nil
                      configuration:BookmarkEditor::SHOW_TREE
                            handler:nil];
  }

};

TEST_F(BookmarkEditorControllerTreeNoNodeTest, NewBookmarkNoNode) {
  [controller_ setDisplayName:@"NEW BOOKMARK"];
  [controller_ setDisplayURL:@"http://NEWURL.com"];
  [controller_ ok:nil];
  const BookmarkNode* new_node = group_bb_->GetChild(5);
  ASSERT_EQ(0, new_node->GetChildCount());
  EXPECT_EQ(new_node->GetTitle(), L"NEW BOOKMARK");
  EXPECT_EQ(new_node->GetURL(), GURL("http://NEWURL.com"));
}
