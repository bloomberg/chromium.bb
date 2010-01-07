// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/bookmark_model_verifier.h"

#include <vector>
#include <stack>

#include "app/tree_node_iterator.h"
#include "base/rand_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using std::string;
using std::wstring;

// static
void BookmarkModelVerifier::ExpectBookmarkInfoMatch(
    const BookmarkNode* expected, const BookmarkNode* actual) {
  EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
  EXPECT_EQ(expected->is_folder(), actual->is_folder());
  EXPECT_EQ(expected->GetURL(), actual->GetURL());
  EXPECT_EQ(expected->GetParent()->IndexOfChild(expected),
            actual->GetParent()->IndexOfChild(actual));
}

BookmarkModelVerifier::BookmarkModelVerifier() {
  verifier_profile_.reset(LiveBookmarksSyncTest::MakeProfile(L"verifier"));
  verifier_ = verifier_profile_->GetBookmarkModel();
}

BookmarkModelVerifier* BookmarkModelVerifier::Create() {
  BookmarkModelVerifier* v = new BookmarkModelVerifier();
  LiveBookmarksSyncTest::BlockUntilLoaded(v->verifier_);
  return v;
}

void BookmarkModelVerifier::ExpectMatch(BookmarkModel* actual) {
  ExpectModelsMatch(verifier_, actual);
}

// static
void BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
    BookmarkModel* expected, BookmarkModel* actual, bool compare_favicon) {
  TreeNodeIterator<const BookmarkNode> e_iterator(expected->root_node());
  TreeNodeIterator<const BookmarkNode> a_iterator(actual->root_node());

  // Pre-order traversal of each model, comparing at each step.
  while (e_iterator.has_next()) {
    const BookmarkNode* e_node = e_iterator.Next();
    ASSERT_TRUE(a_iterator.has_next());
    const BookmarkNode* a_node = a_iterator.Next();
    ExpectBookmarkInfoMatch(e_node, a_node);
    // Get Favicon and compare if compare_favicon flag is true.
    if (compare_favicon) {
      const SkBitmap& e_node_favicon = expected->GetFavIcon(e_node);
      const SkBitmap& a_node_favicon = actual->GetFavIcon(a_node);
      EXPECT_GT(e_node_favicon.getSize(), (size_t) 0);
      EXPECT_EQ(e_node_favicon.getSize(), a_node_favicon.getSize());
      EXPECT_EQ(e_node_favicon.width(), a_node_favicon.width());
      EXPECT_EQ(e_node_favicon.height(), a_node_favicon.height());
      SkAutoLockPixels bitmap_lock_e(e_node_favicon);
      SkAutoLockPixels bitmap_lock_a(a_node_favicon);
      void* e_node_pixel_addr = e_node_favicon.getPixels();
      ASSERT_TRUE(e_node_pixel_addr);
      void* a_node_pixel_addr = a_node_favicon.getPixels();
      ASSERT_TRUE(a_node_pixel_addr);
      EXPECT_EQ(memcmp(e_node_pixel_addr, a_node_pixel_addr,
          e_node_favicon.getSize()), 0);
    }
  }
  ASSERT_FALSE(a_iterator.has_next());
}

void BookmarkModelVerifier::VerifyNoDuplicates(BookmarkModel* model) {
  TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  // Pre-order traversal of model tree, looking for duplicate node at
  // each step.
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    std::vector<const BookmarkNode*> nodes;
    if (node->type() != BookmarkNode::URL)
      continue;
    // Get nodes with same URL.
    model->GetNodesByURL(node->GetURL(), &nodes);
    EXPECT_TRUE(nodes.size() >= 1);
    for (std::vector<const BookmarkNode*>::const_iterator i = nodes.begin(),
         e = nodes.end(); i != e; i++) {
      // Skip if it's same node.
      int64 id = node->id();
      if (id == (*i)->id()) {
        continue;
      } else {
        // Make sure title are not same.
        EXPECT_NE(node->GetTitle(), (*i)->GetTitle());
      }
    }
  }  // end of while
}

void BookmarkModelVerifier::FindNodeInVerifier(BookmarkModel* foreign_model,
                                               const BookmarkNode* foreign_node,
                                               const BookmarkNode** result) {
  // Climb the tree.
  std::stack<int> path;
  const BookmarkNode* walker = foreign_node;
  while (walker != foreign_model->root_node()) {
    path.push(walker->GetParent()->IndexOfChild(walker));
    walker = walker->GetParent();
  }

  // Swing over to the other tree.
  walker = verifier_->root_node();

  // Climb down.
  while (!path.empty()) {
    ASSERT_TRUE(walker->is_folder());
    ASSERT_LT(path.top(), walker->GetChildCount());
    walker = walker->GetChild(path.top());
    path.pop();
  }

  ExpectBookmarkInfoMatch(foreign_node, walker);
  *result = walker;
}

const BookmarkNode* BookmarkModelVerifier::AddGroup(BookmarkModel* model,
    const BookmarkNode* parent, int index, const wstring& title) {
  const BookmarkNode* v_parent = NULL;
  FindNodeInVerifier(model, parent, &v_parent);
  const BookmarkNode* result = model->AddGroup(parent, index, title);
  EXPECT_TRUE(result);
  if (!result)
    return NULL;
  const BookmarkNode* v_node = verifier_->AddGroup(v_parent, index, title);
  EXPECT_TRUE(v_node);
  if (!v_node)
    return NULL;
  ExpectBookmarkInfoMatch(v_node, result);
  return result;
}

const BookmarkNode* BookmarkModelVerifier::AddNonEmptyGroup(
    BookmarkModel* model, const BookmarkNode* parent, int index,
    const wstring& title, int children_count) {
  const BookmarkNode* bm_folder = AddGroup(model, parent, index, title);
  EXPECT_TRUE(bm_folder);
  if (!bm_folder)
    return NULL;
  for (int child_index = 0; child_index < children_count; child_index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 60% of time add bookmarks
    if (random_int > 40) {
      wstring child_bm_title(bm_folder->GetTitle());
      child_bm_title.append(L"-ChildBM");
      child_bm_title.append(IntToWString(index));
      string url("http://www.nofaviconurl");
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* child_nofavicon_bm =
         AddURL(model, bm_folder, child_index, child_bm_title, GURL(url));
      EXPECT_TRUE(child_nofavicon_bm != NULL);
    } else {
      // Remaining % of time - Add Bookmark folders
      wstring child_bmfolder_title(bm_folder->GetTitle());
      child_bmfolder_title.append(L"-ChildBMFolder");
      child_bmfolder_title.append(IntToWString(index));
      const BookmarkNode* child_bm_folder =
          AddGroup(model, bm_folder, child_index, child_bmfolder_title);
      EXPECT_TRUE(child_bm_folder != NULL);
    }
  }
  return bm_folder;
}

const BookmarkNode* BookmarkModelVerifier::AddURL(BookmarkModel* model,
    const BookmarkNode* parent, int index, const wstring& title,
    const GURL& url) {
  const BookmarkNode* v_parent = NULL;
  FindNodeInVerifier(model, parent, &v_parent);
  const BookmarkNode* result = model->AddURL(parent, index, title, url);
  EXPECT_TRUE(result);
  if (!result)
    return NULL;
  const BookmarkNode* v_node = verifier_->AddURL(v_parent, index, title, url);
  EXPECT_TRUE(v_node);
  if (!v_node)
    return NULL;
  ExpectBookmarkInfoMatch(v_node, result);
  return result;
}

void BookmarkModelVerifier::SetTitle(BookmarkModel* model,
                                     const BookmarkNode* node,
                                     const wstring& title) {
  const BookmarkNode* v_node = NULL;
  FindNodeInVerifier(model, node, &v_node);
  model->SetTitle(node, title);
  verifier_->SetTitle(v_node, title);
}

void BookmarkModelVerifier::Move(BookmarkModel* model, const BookmarkNode* node,
                                 const BookmarkNode* new_parent, int index) {
  const BookmarkNode* v_new_parent = NULL;
  const BookmarkNode* v_node = NULL;
  FindNodeInVerifier(model, new_parent, &v_new_parent);
  FindNodeInVerifier(model, node, &v_node);
  model->Move(node, new_parent, index);
  verifier_->Move(v_node, v_new_parent, index);
}

void BookmarkModelVerifier::Remove(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index) {
  const BookmarkNode* v_parent = NULL;
  FindNodeInVerifier(model, parent, &v_parent);
  ExpectBookmarkInfoMatch(parent->GetChild(index), v_parent->GetChild(index));
  model->Remove(parent, index);
  verifier_->Remove(v_parent, index);
}

void BookmarkModelVerifier::SortChildren(BookmarkModel* model,
                                         const BookmarkNode* parent) {
  const BookmarkNode* v_parent = NULL;
  FindNodeInVerifier(model, parent, &v_parent);
  model->SortChildren(parent);
  verifier_->SortChildren(v_parent);
}

void BookmarkModelVerifier::ReverseChildOrder(BookmarkModel* model,
                                         const BookmarkNode* parent) {
  int child_count = parent->GetChildCount();
  if (child_count <= 0)
    return;
  for (int index = 0; index < child_count; index++)
    Move(model, parent->GetChild(index), parent, child_count-index);
}

const BookmarkNode* BookmarkModelVerifier::SetURL(BookmarkModel* model,
                                   const BookmarkNode* node,
                                   const GURL& new_url) {
  const BookmarkNode* v_node = NULL;
  FindNodeInVerifier(model, node, &v_node);
  const BookmarkNode* result = bookmark_utils::ApplyEditsWithNoGroupChange(
      model, node->GetParent(), BookmarkEditor::EditDetails(node),
      node->GetTitle(), new_url, NULL);
  bookmark_utils::ApplyEditsWithNoGroupChange(verifier_, v_node->GetParent(),
      BookmarkEditor::EditDetails(v_node), v_node->GetTitle(), new_url, NULL);
  return result;
}
