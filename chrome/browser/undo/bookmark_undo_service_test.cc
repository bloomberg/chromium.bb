// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/bookmark_undo_service.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

class BookmarkUndoServiceTest : public testing::Test {
 public:
  BookmarkUndoServiceTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  BookmarkModel* GetModel();
  BookmarkUndoService* GetUndoService();

 private:
  scoped_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkUndoServiceTest);
};

BookmarkUndoServiceTest::BookmarkUndoServiceTest() {}

void BookmarkUndoServiceTest::SetUp() {
  profile_.reset(new TestingProfile);
  profile_->CreateBookmarkModel(true);
  test::WaitForBookmarkModelToLoad(GetModel());
}

BookmarkModel* BookmarkUndoServiceTest::GetModel() {
  return BookmarkModelFactory::GetForProfile(profile_.get());
}

BookmarkUndoService* BookmarkUndoServiceTest::GetUndoService() {
  return BookmarkUndoServiceFactory::GetForProfile(profile_.get());
}

void BookmarkUndoServiceTest::TearDown() {
  profile_.reset(NULL);
}

TEST_F(BookmarkUndoServiceTest, AddBookmark) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, ASCIIToUTF16("foo"), GURL("http://www.bar.com"));

  // Undo bookmark creation and test for no bookmarks.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(0, model->other_node()->child_count());

  // Redo bookmark creation and ensure bookmark information is valid.
  undo_service->undo_manager()->Redo();
  const BookmarkNode* node = parent->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));
}

// Test that a bookmark removal action can be undone and redone.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkRemove) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, ASCIIToUTF16("foo"), GURL("http://www.bar.com"));
  model->Remove(parent, 0);

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());

  // Undo the deletion of the only bookmark and check the bookmark values.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(1, model->other_node()->child_count());
  const BookmarkNode* node = parent->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  EXPECT_EQ(1U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(1U, undo_service->undo_manager()->redo_count());

  // Redo the deletion and check that there are no bookmarks left.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(0, model->other_node()->child_count());

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());
}

// Ensure the undo/redo works for editing of bookmark information grouped into
// one action.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkGroupedAction) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* n1 = model->AddURL(model->other_node(),
                                        0,
                                        ASCIIToUTF16("foo"),
                                        GURL("http://www.foo.com"));
  undo_service->undo_manager()->StartGroupingActions();
  model->SetTitle(n1, ASCIIToUTF16("bar"));
  model->SetURL(n1, GURL("http://www.bar.com"));
  undo_service->undo_manager()->EndGroupingActions();

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());

  // Undo the modification of the bookmark and check for the original values.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(1, model->other_node()->child_count());
  const BookmarkNode* node = model->other_node()->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  // Redo the modifications and ensure the newer values are present.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(1, model->other_node()->child_count());
  node = model->other_node()->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("bar"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());
}

// Test moving bookmarks within a folder and between folders.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkMoveWithinFolder) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* n1 = model->AddURL(model->other_node(),
                                        0,
                                        ASCIIToUTF16("foo"),
                                        GURL("http://www.foo.com"));
  const BookmarkNode* n2 = model->AddURL(model->other_node(),
                                        1,
                                        ASCIIToUTF16("moo"),
                                        GURL("http://www.moo.com"));
  const BookmarkNode* n3 = model->AddURL(model->other_node(),
                                        2,
                                        ASCIIToUTF16("bar"),
                                        GURL("http://www.bar.com"));
  model->Move(n1, model->other_node(), 3);

  // Undo the move and check that the nodes are in order.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(model->other_node()->GetChild(0), n1);
  EXPECT_EQ(model->other_node()->GetChild(1), n2);
  EXPECT_EQ(model->other_node()->GetChild(2), n3);

  // Redo the move and check that the first node is in the last position.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(model->other_node()->GetChild(0), n2);
  EXPECT_EQ(model->other_node()->GetChild(1), n3);
  EXPECT_EQ(model->other_node()->GetChild(2), n1);
}

// Test undo of a bookmark moved to a different folder.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkMoveToOtherFolder) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* n1 = model->AddURL(model->other_node(),
                                        0,
                                        ASCIIToUTF16("foo"),
                                        GURL("http://www.foo.com"));
  const BookmarkNode* n2 = model->AddURL(model->other_node(),
                                        1,
                                        ASCIIToUTF16("moo"),
                                        GURL("http://www.moo.com"));
  const BookmarkNode* n3 = model->AddURL(model->other_node(),
                                        2,
                                        ASCIIToUTF16("bar"),
                                        GURL("http://www.bar.com"));
  const BookmarkNode* f1 =
      model->AddFolder(model->other_node(), 3, ASCIIToUTF16("folder"));
  model->Move(n3, f1, 0);

  // Undo the move and check that the bookmark and folder are in place.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(4, model->other_node()->child_count());
  EXPECT_EQ(model->other_node()->GetChild(0), n1);
  EXPECT_EQ(model->other_node()->GetChild(1), n2);
  EXPECT_EQ(model->other_node()->GetChild(2), n3);
  EXPECT_EQ(model->other_node()->GetChild(3), f1);
  EXPECT_EQ(0, f1->child_count());

  // Redo the move back into the folder and check validity.
  undo_service->undo_manager()->Redo();
  ASSERT_EQ(3, model->other_node()->child_count());
  EXPECT_EQ(model->other_node()->GetChild(0), n1);
  EXPECT_EQ(model->other_node()->GetChild(1), n2);
  EXPECT_EQ(model->other_node()->GetChild(2), f1);
  ASSERT_EQ(1, f1->child_count());
  EXPECT_EQ(f1->GetChild(0), n3);
}

// Tests the handling of multiple modifications that include renumbering of the
// bookmark identifiers.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkRenameDelete) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* f1 = model->AddFolder(model->other_node(),
                                           0,
                                           ASCIIToUTF16("folder"));
  model->AddURL(f1, 0, ASCIIToUTF16("foo"), GURL("http://www.foo.com"));
  model->SetTitle(f1, ASCIIToUTF16("Renamed"));
  model->Remove(model->other_node(), 0);

  // Undo the folder removal and ensure the folder and bookmark were restored.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(1, model->other_node()->child_count());
  ASSERT_EQ(1, model->other_node()->GetChild(0)->child_count());
  const BookmarkNode* node = model->other_node()->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("Renamed"));

  node = model->other_node()->GetChild(0)->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  // Undo the title change and ensure the folder was updated even though the
  // id has changed.
  undo_service->undo_manager()->Undo();
  node = model->other_node()->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("folder"));

  // Undo bookmark creation and test for removal of bookmark.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(0, model->other_node()->GetChild(0)->child_count());

  // Undo folder creation and confirm the bookmark model is empty.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(0, model->other_node()->child_count());

  // Redo all the actions and ensure the folder and bookmark are restored.
  undo_service->undo_manager()->Redo(); // folder creation
  undo_service->undo_manager()->Redo(); // bookmark creation
  undo_service->undo_manager()->Redo(); // bookmark title change
  ASSERT_EQ(1, model->other_node()->child_count());
  ASSERT_EQ(1, model->other_node()->GetChild(0)->child_count());
  node = model->other_node()->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("Renamed"));
  node = model->other_node()->GetChild(0)->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  undo_service->undo_manager()->Redo(); // folder deletion
  EXPECT_EQ(0, model->other_node()->child_count());
}

// Test the undo of SortChildren and ReorderChildren.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkReorder) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, ASCIIToUTF16("foo"), GURL("http://www.foo.com"));
  model->AddURL(parent, 1, ASCIIToUTF16("moo"), GURL("http://www.moo.com"));
  model->AddURL(parent, 2, ASCIIToUTF16("bar"), GURL("http://www.bar.com"));
  model->SortChildren(parent);

  // Test the undo of SortChildren.
  undo_service->undo_manager()->Undo();
  const BookmarkNode* node = parent->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  node = parent->GetChild(1);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("moo"));
  EXPECT_EQ(node->url(), GURL("http://www.moo.com"));

  node = parent->GetChild(2);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("bar"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  // Test the redo of SortChildren.
  undo_service->undo_manager()->Redo();
  node = parent->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("bar"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  node = parent->GetChild(1);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  node = parent->GetChild(2);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("moo"));
  EXPECT_EQ(node->url(), GURL("http://www.moo.com"));

}

TEST_F(BookmarkUndoServiceTest, UndoBookmarkRemoveAll) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  // Setup bookmarks in the Other Bookmarks and the Bookmark Bar.
  const BookmarkNode* new_folder;
  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, ASCIIToUTF16("foo"), GURL("http://www.google.com"));
  new_folder= model->AddFolder(parent, 1, ASCIIToUTF16("folder"));
  model->AddURL(new_folder, 0, ASCIIToUTF16("bar"), GURL("http://www.bar.com"));

  parent = model->bookmark_bar_node();
  model->AddURL(parent, 0, ASCIIToUTF16("a"), GURL("http://www.a.com"));
  new_folder = model->AddFolder(parent, 1, ASCIIToUTF16("folder"));
  model->AddURL(new_folder, 0, ASCIIToUTF16("b"), GURL("http://www.b.com"));

  model->RemoveAllUserBookmarks();

  // Test that the undo of RemoveAllUserBookmarks restores all folders and
  // bookmarks.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(2, model->other_node()->child_count());
  EXPECT_EQ(1, model->other_node()->GetChild(1)->child_count());
  const BookmarkNode* node = model->other_node()->GetChild(1)->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("bar"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  ASSERT_EQ(2, model->bookmark_bar_node()->child_count());
  EXPECT_EQ(1, model->bookmark_bar_node()->GetChild(1)->child_count());
  node = model->bookmark_bar_node()->GetChild(1)->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("b"));
  EXPECT_EQ(node->url(), GURL("http://www.b.com"));

  // Test that the redo removes all folders and bookmarks.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(0, model->other_node()->child_count());
  EXPECT_EQ(0, model->bookmark_bar_node()->child_count());
}

TEST_F(BookmarkUndoServiceTest, UndoRemoveFolderWithBookmarks) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  // Setup bookmarks in the Other Bookmarks.
  const BookmarkNode* new_folder;
  const BookmarkNode* parent = model->other_node();
  new_folder = model->AddFolder(parent, 0, ASCIIToUTF16("folder"));
  model->AddURL(new_folder, 0, ASCIIToUTF16("bar"), GURL("http://www.bar.com"));

  model->Remove(parent, 0);

  // Test that the undo restores the bookmark and folder.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(1, model->other_node()->child_count());
  new_folder = model->other_node()->GetChild(0);
  EXPECT_EQ(1, new_folder->child_count());
  const BookmarkNode* node = new_folder->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("bar"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  // Test that the redo restores the bookmark and folder.
  undo_service->undo_manager()->Redo();

  ASSERT_EQ(0, model->other_node()->child_count());

  // Test that the undo after a redo restores the bookmark and folder.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(1, model->other_node()->child_count());
  new_folder = model->other_node()->GetChild(0);
  EXPECT_EQ(1, new_folder->child_count());
  node = new_folder->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("bar"));
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));
}

TEST_F(BookmarkUndoServiceTest, TestUpperLimit) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();
  model->AddObserver(undo_service);

  // This maximum is set in undo_manager.cc
  const size_t kMaxUndoGroups = 100;

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, ASCIIToUTF16("foo"), GURL("http://www.foo.com"));
  for (size_t i = 1; i < kMaxUndoGroups + 1; ++i)
    model->AddURL(parent, i, ASCIIToUTF16("bar"), GURL("http://www.bar.com"));

  EXPECT_EQ(kMaxUndoGroups, undo_service->undo_manager()->undo_count());

  // Undo as many operations as possible.
  while (undo_service->undo_manager()->undo_count())
    undo_service->undo_manager()->Undo();

  EXPECT_EQ(1, parent->child_count());
  const BookmarkNode* node = model->other_node()->GetChild(0);
  EXPECT_EQ(node->GetTitle(), ASCIIToUTF16("foo"));
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));
}

} // namespace
