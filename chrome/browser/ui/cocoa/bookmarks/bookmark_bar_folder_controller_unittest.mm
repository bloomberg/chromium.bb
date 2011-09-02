// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_unittest_helper.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#include "chrome/test/base/model_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {
const int kLotsOfNodesCount = 150;
};

class BookmarkBarFolderControllerTest : public CocoaTest {
 public:
  BookmarkModel* GetModel() {
    return helper_.profile()->GetBookmarkModel();
  }

  BookmarkBarFolderController* CreateController(const BookmarkNode* node) {
    // The button will be owned by the controller.
    scoped_nsobject<BookmarkButton> button(
        [[BookmarkButton alloc] initWithFrame:NSMakeRect(0, 0, 500, 500)]);
    scoped_nsobject<BookmarkButtonCell> cell(
        [[BookmarkButtonCell alloc] initTextCell:@"foo"]);
    [button setCell:cell];
    [cell setBookmarkNode:node];

    BookmarkModel* model = GetModel();
    return [[BookmarkBarFolderController alloc] initWithParentButton:button
                                                       bookmarkModel:model
                                                       barController:nil];
  }

  NSMenu* GetMenu(BookmarkBarFolderController* controller) {
    return [[controller menuBridge]->controller() menu];
  }

 private:
  BrowserTestHelper helper_;
};

TEST_F(BookmarkBarFolderControllerTest, SimpleFolder) {
  // Create the model.
  BookmarkModel* model = GetModel();
  const BookmarkNode* root = model->bookmark_bar_node();

  model->AddURL(root, root->child_count(), ASCIIToUTF16("ignored"),
      GURL("http://example.com"));

  const BookmarkNode* folder = model->AddFolder(root, root->child_count(),
      ASCIIToUTF16("folder"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 1"),
      GURL("http://www.google.com/"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 2"),
      GURL("http://www.chromium.org/"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 3"),
      GURL("http://build.chromium.org/"));

  model->AddURL(root, root->child_count(), ASCIIToUTF16("ignored 2"),
      GURL("http://example2.com"));

  // Create the controller and menu.
  scoped_nsobject<BookmarkBarFolderController> bbfc(CreateController(folder));
  CloseFolderAfterDelay(bbfc, 0.1);
  [bbfc openMenu];

  NSArray* items = [GetMenu(bbfc) itemArray];
  ASSERT_EQ(3u, [items count]);

  EXPECT_NSEQ(@"item 1", [[items objectAtIndex:0] title]);
  EXPECT_NSEQ(@"item 2", [[items objectAtIndex:1] title]);
  EXPECT_NSEQ(@"item 3", [[items objectAtIndex:2] title]);
}

TEST_F(BookmarkBarFolderControllerTest, NestedFolder) {
  // Create the model.
  BookmarkModel* model = GetModel();
  const BookmarkNode* root = model->bookmark_bar_node();

  model->AddURL(root, root->child_count(), ASCIIToUTF16("ignored"),
      GURL("http://example.com"));
  model->AddURL(root, root->child_count(), ASCIIToUTF16("ignored 2"),
      GURL("http://example2.com"));

  const BookmarkNode* folder = model->AddFolder(root, root->child_count(),
      ASCIIToUTF16("folder"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 1"),
      GURL("http://www.google.com/"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 2"),
      GURL("http://www.chromium.org/"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 3"),
      GURL("http://build.chromium.org/"));
  const BookmarkNode* subfolder = model->AddFolder(folder,
      folder->child_count(), ASCIIToUTF16("subfolder"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("item 4"),
      GURL("http://dev.chromium.org/"));

  model->AddURL(subfolder, subfolder->child_count(), ASCIIToUTF16("subitem 1"),
      GURL("http://gmail.com"));
  model->AddURL(subfolder, subfolder->child_count(), ASCIIToUTF16("subitem 2"),
      GURL("http://google.com/+"));

  // Create the controller and menu.
  scoped_nsobject<BookmarkBarFolderController> bbfc(CreateController(folder));
  CloseFolderAfterDelay(bbfc, 0.1);
  [bbfc openMenu];

  NSArray* items = [GetMenu(bbfc) itemArray];
  ASSERT_EQ(5u, [items count]);

  EXPECT_NSEQ(@"item 1", [[items objectAtIndex:0] title]);
  EXPECT_NSEQ(@"item 2", [[items objectAtIndex:1] title]);
  EXPECT_NSEQ(@"item 3", [[items objectAtIndex:2] title]);
  EXPECT_NSEQ(@"subfolder", [[items objectAtIndex:3] title]);
  EXPECT_NSEQ(@"item 4", [[items objectAtIndex:4] title]);

  NSArray* subitems = [[[items objectAtIndex:3] submenu] itemArray];
  ASSERT_EQ(2u, [subitems count]);
  EXPECT_NSEQ(@"subitem 1", [[subitems objectAtIndex:0] title]);
  EXPECT_NSEQ(@"subitem 2", [[subitems objectAtIndex:1] title]);
}
