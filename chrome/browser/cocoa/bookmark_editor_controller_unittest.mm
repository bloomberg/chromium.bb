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

class BookmarkEditorControllerTest : public PlatformTest {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  const BookmarkNode* default_node_;
  const BookmarkNode* default_parent_;
  const char* default_name_;
  std::wstring default_title_;
  scoped_nsobject<BookmarkEditorController> default_controller_;

  BookmarkEditorControllerTest() {
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    default_parent_ = model->GetBookmarkBarNode();
    default_name_ = "http://www.zim-bop-a-dee.com/";
    default_title_ = L"ooh title";
    const BookmarkNode* default_node = model->AddURL(default_parent_, 0,
                                                     default_title_,
                                                     GURL(default_name_));
    default_controller_.reset([[BookmarkEditorController alloc]
                                initWithParentWindow:cocoa_helper_.window()
                                             profile:helper_.profile()
                                              parent:default_parent_
                                                node:default_node
                                       configuration:BookmarkEditor::NO_TREE
                                             handler:nil]);
    [default_controller_ window];  // Forces a nib load
  }
};

TEST_F(BookmarkEditorControllerTest, NoNodeNoTree) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* node = NULL;

  scoped_nsobject<BookmarkEditorController>
    controller([[BookmarkEditorController alloc]
                 initWithParentWindow:cocoa_helper_.window()
                              profile:helper_.profile()
                               parent:parent
                                 node:node
                        configuration:BookmarkEditor::NO_TREE
                              handler:nil]);

  EXPECT_NE((NSWindow*)nil, [controller window]);  // Forces a nib load
  EXPECT_EQ(@"", [controller displayName]);
  EXPECT_EQ(@"", [controller displayURL]);
  EXPECT_FALSE([controller okButtonEnabled]);
}

TEST_F(BookmarkEditorControllerTest, YesNodeShowTree) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const char* url_name = "http://www.zim-bop-a-dee.com/";
  const BookmarkNode* node = model->AddURL(parent, 0, default_title_,
                                           GURL(url_name));

  scoped_nsobject<BookmarkEditorController>
    controller([[BookmarkEditorController alloc]
                 initWithParentWindow:cocoa_helper_.window()
                              profile:helper_.profile()
                               parent:parent
                                 node:node
                        configuration:BookmarkEditor::SHOW_TREE
                              handler:nil]);

  EXPECT_NE((NSWindow*)nil, [controller window]);  // Forces a nib load
  EXPECT_TRUE([base::SysWideToNSString(default_title_)
                isEqual:[controller displayName]]);
  EXPECT_TRUE([[NSString stringWithCString:url_name
                                  encoding:NSUTF8StringEncoding]
                isEqual:[controller displayURL]]);
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
  EXPECT_FALSE([default_controller_ okButtonEnabled]);
  // Then little mix of both.
  [default_controller_ setDisplayName:@"name"];
  EXPECT_TRUE([default_controller_ okButtonEnabled]);
  [default_controller_ setDisplayURL:@""];
  EXPECT_FALSE([default_controller_ okButtonEnabled]);
}
