// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_handset_view_controller.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

using bookmarks::BookmarkNode;

// A partial mock subclass that doesn't load any heavy weight subclasses.
@interface MockBookmarkHomeHandsetViewController
    : BookmarkHomeHandsetViewController
@end

@implementation MockBookmarkHomeHandsetViewController

- (void)hideEditingBarAnimated:(BOOL)animated {
  // Do nothing.
  // The animation would delay the release of the
  // BookmarkHomeHandsetViewController and make the test fail by keeping
  // the view controller alive after the test is shutdown leading
  // to DCHECK failure on the sign in manager.
}

- (void)ensureAllViewExists {
  // Do nothing.
}
- (void)loadImageService {
  // Do nothing.
}
@end

namespace {

using BookmarkHomeViewControllerTest = BookmarkIOSUnitTest;

TEST_F(BookmarkHomeViewControllerTest, DeleteNodesUpdatesEditNodes) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, @"a");
  const BookmarkNode* b = AddBookmark(mobileNode, @"b");
  const BookmarkNode* f2 = AddFolder(mobileNode, @"f2");

  const BookmarkNode* f1a = AddBookmark(f1, @"f1a");
  AddBookmark(f1, @"f1b");
  AddBookmark(f1, @"f1c");
  const BookmarkNode* f2a = AddBookmark(f2, @"f2a");
  AddBookmark(f2, @"f2b");

  std::set<const BookmarkNode*> toDelete;
  toDelete.insert(b);
  toDelete.insert(f1a);
  toDelete.insert(f1);
  toDelete.insert(f2a);

  base::scoped_nsobject<MockBookmarkHomeHandsetViewController> controller(
      [[MockBookmarkHomeHandsetViewController alloc]
          initWithLoader:nil
            browserState:chrome_browser_state_.get()]);

  [controller resetEditNodes];
  [controller insertEditNode:f1 atIndexPath:nil];
  [controller insertEditNode:a atIndexPath:nil];
  [controller insertEditNode:f2 atIndexPath:nil];

  bookmark_utils_ios::DeleteBookmarks(toDelete, _bookmarkModel);

  // After the deletion, only 'a' and 'f2' should be left.
  std::set<const BookmarkNode*> editingNodes = [controller editNodes];
  EXPECT_EQ(editingNodes.size(), 2u);
  EXPECT_TRUE(editingNodes.find(a) != editingNodes.end());
  EXPECT_TRUE(editingNodes.find(f2) != editingNodes.end());
}

}  // anonymous namespace
