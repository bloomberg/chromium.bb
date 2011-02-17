// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#include "chrome/browser/ui/browser.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeBookmarkMenuController : BookmarkMenuCocoaController {
 @public
  BrowserTestHelper* helper_;
  const BookmarkNode* nodes_[2];
  BOOL opened_[2];
  BOOL opened_new_foreground_tab;
  BOOL opened_new_window;
  BOOL opened_off_the_record;
}
@end

@implementation FakeBookmarkMenuController

- (id)init {
  if ((self = [super init])) {
    string16 empty;
    helper_ = new BrowserTestHelper();
    BookmarkModel* model = helper_->browser()->profile()->GetBookmarkModel();
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    nodes_[0] = model->AddURL(bookmark_bar, 0, empty, GURL("http://0.com"));
    nodes_[1] = model->AddURL(bookmark_bar, 1, empty, GURL("http://1.com"));
  }
  return self;
}

- (void)dealloc {
  delete helper_;
  [super dealloc];
}

- (const BookmarkNode*)nodeForIdentifier:(int)identifier {
  if ((identifier < 0) || (identifier >= 2))
    return NULL;
  return nodes_[identifier];
}

- (void)openURLForNode:(const BookmarkNode*)node {
  std::string url = node->GetURL().possibly_invalid_spec();
  if (url.find("http://0.com") != std::string::npos)
    opened_[0] = YES;
  if (url.find("http://1.com") != std::string::npos)
    opened_[1] = YES;
}

- (void)openAll:(NSInteger)tag
    withDisposition:(WindowOpenDisposition)disposition {
  if (disposition == NEW_FOREGROUND_TAB) {
    opened_new_foreground_tab = YES;
  } else if (disposition == NEW_WINDOW) {
    opened_new_window = YES;
  } else if (disposition == OFF_THE_RECORD) {
    opened_off_the_record = YES;
  }
}

@end  // FakeBookmarkMenuController


TEST(BookmarkMenuCocoaControllerTest, TestOpenItem) {
  FakeBookmarkMenuController *c = [[FakeBookmarkMenuController alloc] init];
  NSMenuItem *item = [[[NSMenuItem alloc] init] autorelease];
  for (int i = 0; i < 2; i++) {
    [item setTag:i];
    ASSERT_EQ(c->opened_[i], NO);
    [c openBookmarkMenuItem:item];
    ASSERT_NE(c->opened_[i], NO);
  }

  // Test three versions of 'Open All Bookmarks' item. tag id means nothing.
  // I just reset the tag id to zero.
  [item setTag:0];
  EXPECT_EQ(c->opened_new_foreground_tab, NO);
  [c openAllBookmarks:item];
  EXPECT_NE(c->opened_new_foreground_tab, NO);

  EXPECT_EQ(c->opened_new_window, NO);
  [c openAllBookmarksNewWindow:item];
  EXPECT_NE(c->opened_new_window, NO);

  EXPECT_EQ(c->opened_off_the_record, NO);
  [c openAllBookmarksIncognitoWindow:item];
  EXPECT_NE(c->opened_off_the_record, NO);

  [c release];
}
