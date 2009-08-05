// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/test/live_sync/bookmark_model_verifier.h"

#include <vector>
#include <stack>

#include "app/tree_node_iterator.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/test/live_sync/live_bookmarks_sync_test.h"
#include "testing/gtest/include/gtest/gtest.h"

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
void BookmarkModelVerifier::ExpectModelsMatch(
    BookmarkModel* expected, BookmarkModel* actual) {
  TreeNodeIterator<const BookmarkNode> e_iterator(expected->root_node());
  TreeNodeIterator<const BookmarkNode> a_iterator(actual->root_node());

  // Pre-order traversal of each model, comparing at each step.
  while (e_iterator.has_next()) {
    const BookmarkNode* e_node = e_iterator.Next();
    ASSERT_TRUE(a_iterator.has_next());
    const BookmarkNode* a_node = a_iterator.Next();
    ExpectBookmarkInfoMatch(e_node, a_node);
  }
  ASSERT_FALSE(a_iterator.has_next());
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
    const BookmarkNode* parent, int index, const string16& title) {
  const BookmarkNode* v_parent = NULL;
  FindNodeInVerifier(model, parent, &v_parent);
  const BookmarkNode* result = model->AddGroup(parent, index, title);
  EXPECT_TRUE(result);
  if (!result) return NULL;
  const BookmarkNode* v_node = verifier_->AddGroup(v_parent, index, title);
  EXPECT_TRUE(v_node);
  if (!v_node) return NULL;
  ExpectBookmarkInfoMatch(v_node, result);
  return result;
}

const BookmarkNode* BookmarkModelVerifier::AddURL(BookmarkModel* model,
    const BookmarkNode* parent, int index, const string16& title,
    const GURL& url) {
  const BookmarkNode* v_parent = NULL;
  FindNodeInVerifier(model, parent, &v_parent);
  const BookmarkNode* result = model->AddURL(parent, index, title, url);
  EXPECT_TRUE(result);
  if (!result) return NULL;
  const BookmarkNode* v_node = verifier_->AddURL(v_parent, index, title, url);
  EXPECT_TRUE(v_node);
  if (!v_node) return NULL;
  ExpectBookmarkInfoMatch(v_node, result);
  return result;
}

void BookmarkModelVerifier::SetTitle(BookmarkModel* model,
                                     const BookmarkNode* node, 
                                     const string16& title) {
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

void BookmarkModelVerifier::SetURL(BookmarkModel* model,
                                   const BookmarkNode* node,
                                   const GURL& new_url) {
  const BookmarkNode* v_node = NULL;
  FindNodeInVerifier(model, node, &v_node);
  bookmark_utils::ApplyEditsWithNoGroupChange(model, node->GetParent(),
      node, node->GetTitle(), new_url, NULL);
  bookmark_utils::ApplyEditsWithNoGroupChange(verifier_, v_node->GetParent(),
      v_node, v_node->GetTitle(), new_url, NULL);
}

#endif  // CHROME_PERSONALIZATION
