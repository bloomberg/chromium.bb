// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/views/controls/menu/menu_runner.h"

class BookmarkMenuDelegateTest : public BrowserWithTestWindowTest {
 public:
  BookmarkMenuDelegateTest() : model_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    profile()->CreateBookmarkModel(true);

    model_ = BookmarkModelFactory::GetForProfile(profile());
    test::WaitForBookmarkModelToLoad(model_);

    AddTestData();

    bookmark_menu_delegate_.reset(
        new BookmarkMenuDelegate(browser(), NULL, NULL, 0));
  }

  virtual void TearDown() OVERRIDE {
    // Since we never show the menu we need to pass the MenuItemView to
    // MenuRunner so that the MenuItemView is destroyed.
    views::MenuRunner menu_runner(bookmark_menu_delegate_->menu());
    bookmark_menu_delegate_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  BookmarkModel* model_;

  scoped_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

 private:
  std::string base_path() const { return "file:///c:/tmp/"; }

  // Creates the following structure:
  // bookmark bar node
  //   a
  //   F1
  //    f1a
  //    F11
  //     f11a
  //   F2
  // other node
  //   oa
  //   OF1
  //     of1a
  void AddTestData() {
    const BookmarkNode* bb_node = model_->bookmark_bar_node();
    std::string test_base = base_path();
    model_->AddURL(bb_node, 0, ASCIIToUTF16("a"), GURL(test_base + "a"));
    const BookmarkNode* f1 = model_->AddFolder(bb_node, 1, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddFolder(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    model_->AddFolder(bb_node, 2, ASCIIToUTF16("F2"));

    // Children of the other node.
    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("oa"),
                   GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model_->AddFolder(model_->other_node(), 1, ASCIIToUTF16("OF1"));
    model_->AddURL(of1, 0, ASCIIToUTF16("of1a"), GURL(test_base + "of1a"));
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuDelegateTest);
};

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
TEST_F(BookmarkMenuDelegateTest, RemoveBookmarks) {
  views::MenuDelegate test_delegate;
  const BookmarkNode* node = model_->bookmark_bar_node()->GetChild(1);
  bookmark_menu_delegate_->Init(&test_delegate, NULL, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                bookmark_utils::LAUNCH_NONE);
  std::vector<const BookmarkNode*> nodes_to_remove;
  nodes_to_remove.push_back(node->GetChild(1));
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  bookmark_menu_delegate_->DidRemoveBookmarks();
}
