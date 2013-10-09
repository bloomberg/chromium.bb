// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"

class BookmarkMenuDelegateTest : public BrowserWithTestWindowTest {
 public:
  BookmarkMenuDelegateTest() : model_(NULL) {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    profile()->CreateBookmarkModel(true);

    model_ = BookmarkModelFactory::GetForProfile(profile());
    test::WaitForBookmarkModelToLoad(model_);

    AddTestData();
  }

  virtual void TearDown() OVERRIDE {
    if (bookmark_menu_delegate_.get()) {
      // Since we never show the menu we need to pass the MenuItemView to
      // MenuRunner so that the MenuItemView is destroyed.
      views::MenuRunner menu_runner(bookmark_menu_delegate_->menu());
      bookmark_menu_delegate_.reset();
    }
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void NewDelegate(int min_menu_id, int max_menu_id) {
    // Destroy current menu if available, see comments in TearDown().
    if (bookmark_menu_delegate_.get())
      views::MenuRunner menu_runner(bookmark_menu_delegate_->menu());

    bookmark_menu_delegate_.reset(
        new BookmarkMenuDelegate(browser(), NULL, NULL,
                                 min_menu_id, max_menu_id));
  }

  void NewAndInitDelegateForPermanent(int min_menu_id,
                                      int max_menu_id) {
    const BookmarkNode* node = model_->bookmark_bar_node();
    NewDelegate(min_menu_id, max_menu_id);
    bookmark_menu_delegate_->Init(&test_delegate_, NULL, node, 0,
                                  BookmarkMenuDelegate::SHOW_PERMANENT_FOLDERS,
                                  BOOKMARK_LAUNCH_LOCATION_NONE);
  }

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

  views::MenuDelegate test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuDelegateTest);
};

// Verifies WillRemoveBookmarks() doesn't attempt to access MenuItemViews that
// have since been deleted.
TEST_F(BookmarkMenuDelegateTest, RemoveBookmarks) {
  views::MenuDelegate test_delegate;
  const BookmarkNode* node = model_->bookmark_bar_node()->GetChild(1);
  NewDelegate(0, kint32max);
  bookmark_menu_delegate_->Init(&test_delegate, NULL, node, 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BOOKMARK_LAUNCH_LOCATION_NONE);
  std::vector<const BookmarkNode*> nodes_to_remove;
  nodes_to_remove.push_back(node->GetChild(1));
  bookmark_menu_delegate_->WillRemoveBookmarks(nodes_to_remove);
  nodes_to_remove.clear();
  bookmark_menu_delegate_->DidRemoveBookmarks();
}

// Verifies menu ID's of items in menu fall within the specified range.
TEST_F(BookmarkMenuDelegateTest, MenuIdRange) {
  // Start with maximum menu Id of 10 - the number of items that AddTestData()
  // populated.  Everything should be created.
  NewAndInitDelegateForPermanent(0, 10);
  views::MenuItemView* root_item = bookmark_menu_delegate_->menu();
  ASSERT_TRUE(root_item->HasSubmenu());
  EXPECT_EQ(4, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(5, root_item->GetSubmenu()->child_count());  // Includes separator.
  views::MenuItemView* F1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(F1_item->HasSubmenu());
  EXPECT_EQ(2, F1_item->GetSubmenu()->GetMenuItemCount());
  views::MenuItemView* F11_item = F1_item->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(F11_item->HasSubmenu());
  EXPECT_EQ(1, F11_item->GetSubmenu()->GetMenuItemCount());
  views::MenuItemView* other_item = root_item->GetSubmenu()->GetMenuItemAt(3);
  ASSERT_TRUE(other_item->HasSubmenu());
  EXPECT_EQ(2, other_item->GetSubmenu()->GetMenuItemCount());
  views::MenuItemView* OF1_item = other_item->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_TRUE(OF1_item->HasSubmenu());
  EXPECT_EQ(1, OF1_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum 9.  "of1a" item should not be created.
  NewAndInitDelegateForPermanent(0, 9);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(4, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(5, root_item->GetSubmenu()->child_count());  // Includes separator.
  other_item = root_item->GetSubmenu()->GetMenuItemAt(3);
  OF1_item = other_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(0, OF1_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum 8.  "OF1" submenu should not be created.
  NewAndInitDelegateForPermanent(0, 8);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(4, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(5, root_item->GetSubmenu()->child_count());  // Includes separator.
  other_item = root_item->GetSubmenu()->GetMenuItemAt(3);
  EXPECT_EQ(1, other_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum 7.  "Other" submenu should be empty.
  NewAndInitDelegateForPermanent(0, 7);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(4, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(5, root_item->GetSubmenu()->child_count());  // Includes separator.
  other_item = root_item->GetSubmenu()->GetMenuItemAt(3);
  EXPECT_EQ(0, other_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum to 6.  "Other" submenu should not be created, and no
  // separator.
  NewAndInitDelegateForPermanent(0, 6);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(3, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(3, root_item->GetSubmenu()->child_count());  // No separator.

  // Reduce maximum 5.  "F2" and "Other" submenus shouldn't be created.
  NewAndInitDelegateForPermanent(0, 5);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(2, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(2, root_item->GetSubmenu()->child_count());  // No separator.
  F1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  F11_item = F1_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(1, F11_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum to 4.  "f11a" item and "F2" and "Other" submenus should
  // not be created.
  NewAndInitDelegateForPermanent(0, 4);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(2, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(2, root_item->GetSubmenu()->child_count());  // No separator.
  F1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  F11_item = F1_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(0, F11_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum to 3.  "F11", "F2" and "Other" submenus should not be
  // created.
  NewAndInitDelegateForPermanent(0, 3);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(2, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(2, root_item->GetSubmenu()->child_count());  // No separator.
  F1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(views::MenuItemView::SUBMENU, F1_item->GetType());
  EXPECT_EQ(1, F1_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum 2.  Only "a" item and empty "F1" submenu should be created.
  NewAndInitDelegateForPermanent(0, 2);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(2, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(2, root_item->GetSubmenu()->child_count());  // No separator.
  F1_item = root_item->GetSubmenu()->GetMenuItemAt(1);
  EXPECT_EQ(views::MenuItemView::SUBMENU, F1_item->GetType());
  EXPECT_EQ(0, F1_item->GetSubmenu()->GetMenuItemCount());

  // Reduce maximum to 1.  Only "a" item should be created.
  NewAndInitDelegateForPermanent(0, 1);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(1, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(1, root_item->GetSubmenu()->child_count());  // No separator.

  // Verify correct handling of integer overflow with range, set kint32max as
  // maximum and 1 less as minimum.  Only "a" item should be created.
  NewAndInitDelegateForPermanent(kint32max - 1, kint32max);
  root_item = bookmark_menu_delegate_->menu();
  EXPECT_EQ(1, root_item->GetSubmenu()->GetMenuItemCount());
  EXPECT_EQ(1, root_item->GetSubmenu()->child_count());  // No separator.
}
