// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#endif

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  virtual void OpenURL(const GURL& url,
                       const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    urls_.push_back(url);
  }

  std::vector<GURL> urls_;
};

class BookmarkContextMenuControllerTest : public testing::Test {
 public:
  BookmarkContextMenuControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        model_(NULL) {
  }

  virtual void SetUp() {
#if defined(OS_WIN)
    BookmarkBarView::testing_ = true;
#endif

    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    profile_->BlockUntilBookmarkModelLoaded();

    model_ = profile_->GetBookmarkModel();

    AddTestData();
  }

  virtual void TearDown() {
#if defined(OS_WIN)
    BookmarkBarView::testing_ = false;
#endif

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }

 protected:
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  scoped_ptr<TestingProfile> profile_;
  BookmarkModel* model_;
  TestingPageNavigator navigator_;

 private:
  // Creates the following structure:
  // a
  // F1
  //  f1a
  //  F11
  //   f11a
  // F2
  // F3
  // F4
  //   f4a
  void AddTestData() {
    std::string test_base = "file:///c:/tmp/";

    model_->AddURL(model_->GetBookmarkBarNode(), 0, ASCIIToUTF16("a"),
                   GURL(test_base + "a"));
    const BookmarkNode* f1 =
        model_->AddFolder(model_->GetBookmarkBarNode(), 1, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddFolder(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    model_->AddFolder(model_->GetBookmarkBarNode(), 2, ASCIIToUTF16("F2"));
    model_->AddFolder(model_->GetBookmarkBarNode(), 3, ASCIIToUTF16("F3"));
    const BookmarkNode* f4 =
        model_->AddFolder(model_->GetBookmarkBarNode(), 4, ASCIIToUTF16("F4"));
    model_->AddURL(f4, 0, ASCIIToUTF16("f4a"), GURL(test_base + "f4a"));
  }
};

// Tests Deleting from the menu.
TEST_F(BookmarkContextMenuControllerTest, DeleteURL) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(0));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  GURL url = model_->GetBookmarkBarNode()->GetChild(0)->GetURL();
  ASSERT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  // Delete the URL.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_REMOVE);
  // Model shouldn't have URL anymore.
  ASSERT_FALSE(model_->IsBookmarked(url));
}

// Tests open all on a folder with a couple of bookmarks.
TEST_F(BookmarkContextMenuControllerTest, OpenAll) {
  const BookmarkNode* folder = model_->GetBookmarkBarNode()->GetChild(1);
  bookmark_utils::OpenAll(
      NULL, profile_.get(), &navigator_, folder, NEW_FOREGROUND_TAB);

  // Should have navigated to F1's child, but not F11's child.
  ASSERT_EQ(static_cast<size_t>(1), navigator_.urls_.size());
  ASSERT_TRUE(folder->GetChild(0)->GetURL() == navigator_.urls_[0]);
}

// Tests the enabled state of the menus when supplied an empty vector.
TEST_F(BookmarkContextMenuControllerTest, EmptyNodes) {
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, model_->other_node(),
      std::vector<const BookmarkNode*>());
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with a single
// url.
TEST_F(BookmarkContextMenuControllerTest, SingleURL) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(0));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// urls.
TEST_F(BookmarkContextMenuControllerTest, MultipleURLs) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(0));
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(1)->GetChild(0));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied an vector with a single
// folder.
TEST_F(BookmarkContextMenuControllerTest, SingleFolder) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(2));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, all of which are empty.
TEST_F(BookmarkContextMenuControllerTest, MultipleEmptyFolders) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(2));
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(3));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, some of which contain URLs.
TEST_F(BookmarkContextMenuControllerTest, MultipleFoldersWithURLs) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(3));
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(4));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of open incognito.
TEST_F(BookmarkContextMenuControllerTest, DisableIncognito) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(0));
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes);
  profile_->set_incognito(true);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_INCOGNITO));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
}

// Tests that you can't remove/edit when showing the other node.
TEST_F(BookmarkContextMenuControllerTest, DisabledItemsWithOtherNode) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->other_node());
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, nodes[0], nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_EDIT));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
}

// Tests the enabled state of the menus when supplied an empty vector and null
// parent.
TEST_F(BookmarkContextMenuControllerTest, EmptyNodesNullParent) {
  BookmarkContextMenuController controller(
      NULL, NULL, profile_.get(), NULL, NULL,
      std::vector<const BookmarkNode*>());
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

TEST_F(BookmarkContextMenuControllerTest, CutCopyPasteNode) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->GetBookmarkBarNode()->GetChild(0));
  scoped_ptr<BookmarkContextMenuController> controller(
      new BookmarkContextMenuController(
          NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_COPY));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_CUT));

  // Copy the URL.
  controller->ExecuteCommand(IDC_COPY);

  controller.reset(new BookmarkContextMenuController(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes));
  int old_count = model_->GetBookmarkBarNode()->child_count();
  controller->ExecuteCommand(IDC_PASTE);

  ASSERT_TRUE(model_->GetBookmarkBarNode()->GetChild(1)->is_url());
  ASSERT_EQ(old_count + 1, model_->GetBookmarkBarNode()->child_count());
  ASSERT_EQ(model_->GetBookmarkBarNode()->GetChild(0)->GetURL(),
            model_->GetBookmarkBarNode()->GetChild(1)->GetURL());

  controller.reset(new BookmarkContextMenuController(
      NULL, NULL, profile_.get(), NULL, nodes[0]->parent(), nodes));
  // Cut the URL.
  controller->ExecuteCommand(IDC_CUT);
  ASSERT_TRUE(model_->GetBookmarkBarNode()->GetChild(0)->is_url());
  ASSERT_TRUE(model_->GetBookmarkBarNode()->GetChild(1)->is_folder());
  ASSERT_EQ(old_count, model_->GetBookmarkBarNode()->child_count());
}
