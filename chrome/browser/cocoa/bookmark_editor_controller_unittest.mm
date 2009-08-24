// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkEditorControllerTest : public testing::Test {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkEditorController> bar_;
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
}

TEST_F(BookmarkEditorControllerTest, YesNodeShowTree) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const char* url_name = "http://www.zim-bop-a-dee.com/";
  const BookmarkNode* node = model->AddURL(parent, 0, L"ooh title",
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
  EXPECT_TRUE([@"ooh title" isEqual:[controller displayName]]);
  EXPECT_TRUE([[NSString stringWithCString:url_name
                                  encoding:NSUTF8StringEncoding]
                isEqual:[controller displayURL]]);
}

TEST_F(BookmarkEditorControllerTest, UserEditsStuff) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const char* url_name = "http://www.zim-bop-a-dee.com/";
  const BookmarkNode* node = model->AddURL(parent, 0, L"ooh title",
                                           GURL(url_name));

  scoped_nsobject<BookmarkEditorController>
    controller([[BookmarkEditorController alloc]
                 initWithParentWindow:cocoa_helper_.window()
                              profile:helper_.profile()
                               parent:parent
                                 node:node
                        configuration:BookmarkEditor::NO_TREE
                              handler:nil]);

  EXPECT_NE((NSWindow*)nil, [controller window]);  // Forces a nib load

  // No change.
  [controller ok:nil];
  const BookmarkNode* child = parent->GetChild(0);
  EXPECT_EQ(child->GetTitle(), L"ooh title");
  EXPECT_EQ(child->GetURL(), GURL(url_name));

  // Change just the title.
  [controller setDisplayName:@"whamma jamma bamma"];
  [controller ok:nil];
  child = parent->GetChild(0);
  EXPECT_EQ(child->GetTitle(), L"whamma jamma bamma");
  EXPECT_EQ(child->GetURL(), GURL(url_name));

  // Change just the URL
  EXPECT_TRUE([controller okButtonEnabled]);
  [controller setDisplayURL:@"http://yellow-sneakers.com/"];
  EXPECT_TRUE([controller okButtonEnabled]);
  [controller ok:nil];
  child = parent->GetChild(0);
  EXPECT_EQ(child->GetTitle(), L"whamma jamma bamma");
  EXPECT_EQ(child->GetURL(), GURL("http://yellow-sneakers.com/"));

  // Give it a URL which needs fixen up to be valid
  // (e.g. http:// prefix added)
  [controller setDisplayURL:@"x"];
  [controller ok:nil];
  child = parent->GetChild(0);
  EXPECT_TRUE(child->GetURL().is_valid());

  // Confirm OK button enabled/disabled as appropriate.
  EXPECT_TRUE([controller okButtonEnabled]);
  [controller setDisplayURL:@""];
  EXPECT_FALSE([controller okButtonEnabled]);
  [controller setDisplayURL:@"http://www.cnn.com"];
  EXPECT_TRUE([controller okButtonEnabled]);
}
