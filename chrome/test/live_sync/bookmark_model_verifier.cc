// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/bookmark_model_verifier.h"

#include <vector>
#include <stack>

#include "app/tree_node_iterator.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
bool BookmarkModelVerifier::NodesMatch(const BookmarkNode* node_a,
                                       const BookmarkNode* node_b) {
  if (node_a == NULL || node_b == NULL)
    return node_a == node_b;
  bool ret_val = true;
  ret_val = ret_val && (node_a->GetTitle() == node_b->GetTitle());
  ret_val = ret_val && (node_a->is_folder() == node_b->is_folder());
  ret_val = ret_val && (node_a->GetURL() == node_b->GetURL());
  ret_val = ret_val && (node_a->GetParent()->IndexOfChild(node_a) ==
                        node_b->GetParent()->IndexOfChild(node_b));
  return ret_val;
}

// static
bool BookmarkModelVerifier::ModelsMatch(BookmarkModel* model_a,
                                        BookmarkModel* model_b,
                                        bool compare_favicons) {
  bool ret_val = true;
  TreeNodeIterator<const BookmarkNode> iterator_a(model_a->root_node());
  TreeNodeIterator<const BookmarkNode> iterator_b(model_b->root_node());
  while (iterator_a.has_next()) {
    const BookmarkNode* node_a = iterator_a.Next();
    EXPECT_TRUE(iterator_b.has_next());
    const BookmarkNode* node_b = iterator_b.Next();
    ret_val = ret_val && NodesMatch(node_a, node_b);
    if (compare_favicons) {
      const SkBitmap& bitmap_a = model_a->GetFavIcon(node_a);
      const SkBitmap& bitmap_b = model_b->GetFavIcon(node_b);
      ret_val = ret_val && FaviconsMatch(bitmap_a, bitmap_b);
    }
  }
  ret_val = ret_val && (!iterator_b.has_next());
  return ret_val;
}

bool BookmarkModelVerifier::FaviconsMatch(const SkBitmap& bitmap_a,
                                          const SkBitmap& bitmap_b) {
  if ((bitmap_a.getSize() == (size_t) 0) ||
      (bitmap_a.getSize() != bitmap_b.getSize()) ||
      (bitmap_a.width() != bitmap_b.width()) ||
      (bitmap_a.height() != bitmap_b.height()))
    return false;
  SkAutoLockPixels bitmap_lock_a(bitmap_a);
  SkAutoLockPixels bitmap_lock_b(bitmap_b);
  void* node_pixel_addr_a = bitmap_a.getPixels();
  EXPECT_TRUE(node_pixel_addr_a);
  void* node_pixel_addr_b = bitmap_b.getPixels();
  EXPECT_TRUE(node_pixel_addr_b);
  return (memcmp(node_pixel_addr_a,
                 node_pixel_addr_b,
                 bitmap_a.getSize()) ==  0);
}

bool BookmarkModelVerifier::ContainsDuplicateBookmarks(BookmarkModel* model) {
  TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->type() != BookmarkNode::URL)
      continue;
    std::vector<const BookmarkNode*> nodes;
    model->GetNodesByURL(node->GetURL(), &nodes);
    EXPECT_TRUE(nodes.size() >= 1);
    for (std::vector<const BookmarkNode*>::const_iterator it = nodes.begin();
         it != nodes.end(); ++it) {
      if (node->id() != (*it)->id() &&
          node->GetParent() == (*it)->GetParent() &&
          node->GetTitle() == (*it)->GetTitle()){
        return true;
      }
    }
  }
  return false;
}

// static
int BookmarkModelVerifier::CountNodesWithTitlesMatching(
    BookmarkModel* model,
    BookmarkNode::Type node_type,
    const string16& title) {
  TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  // Walk through the model tree looking for bookmark nodes of node type
  // |node_type| whose titles match |title|.
  int count = 0;
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if ((node->type() == node_type) && (node->GetTitle() == title))
      ++count;
  }
  return count;
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
  walker = verifier_model_->root_node();

  // Climb down.
  while (!path.empty()) {
    ASSERT_TRUE(walker->is_folder());
    ASSERT_LT(path.top(), walker->GetChildCount());
    walker = walker->GetChild(path.top());
    path.pop();
  }

  ASSERT_TRUE(NodesMatch(foreign_node, walker));
  *result = walker;
}

const BookmarkNode* BookmarkModelVerifier::AddGroup(BookmarkModel* model,
    const BookmarkNode* parent, int index, const string16& title) {
  const BookmarkNode* result = model->AddGroup(parent, index, title);
  EXPECT_TRUE(result);
  if (!result)
    return NULL;
  if (use_verifier_model_) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    const BookmarkNode* v_node = verifier_model_->AddGroup(
        v_parent, index, title);
    EXPECT_TRUE(v_node);
    if (!v_node)
      return NULL;
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

const BookmarkNode* BookmarkModelVerifier::AddURL(BookmarkModel* model,
                                                  const BookmarkNode* parent,
                                                  int index,
                                                  const string16& title,
                                                  const GURL& url) {
  const BookmarkNode* result = model->AddURL(parent, index, title, url);
  EXPECT_TRUE(result);
  if (!result)
    return NULL;
  if (use_verifier_model_) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    const BookmarkNode* v_node =
        verifier_model_->AddURL(v_parent, index, title, url);
    EXPECT_TRUE(v_node);
    if (!v_node)
      return NULL;
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

void BookmarkModelVerifier::SetTitle(BookmarkModel* model,
                                     const BookmarkNode* node,
                                     const string16& title) {
  if (use_verifier_model_) {
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    verifier_model_->SetTitle(v_node, title);
  }
  model->SetTitle(node, title);
}

void BookmarkModelVerifier::Move(BookmarkModel* model,
                                 const BookmarkNode* node,
                                 const BookmarkNode* new_parent,
                                 int index) {
  if (use_verifier_model_) {
    const BookmarkNode* v_new_parent = NULL;
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(model, new_parent, &v_new_parent);
    FindNodeInVerifier(model, node, &v_node);
    verifier_model_->Move(v_node, v_new_parent, index);
  }
  model->Move(node, new_parent, index);
}

void BookmarkModelVerifier::Remove(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index) {
  if (use_verifier_model_) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    ASSERT_TRUE(NodesMatch(parent->GetChild(index), v_parent->GetChild(index)));
    verifier_model_->Remove(v_parent, index);
  }
  model->Remove(parent, index);
}

void BookmarkModelVerifier::SortChildren(BookmarkModel* model,
                                         const BookmarkNode* parent) {
  if (use_verifier_model_) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(model, parent, &v_parent);
    verifier_model_->SortChildren(v_parent);
  }
  model->SortChildren(parent);
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
  if (use_verifier_model_) {
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    bookmark_utils::ApplyEditsWithNoGroupChange(
        verifier_model_, v_node->GetParent(),
        BookmarkEditor::EditDetails(v_node), v_node->GetTitle(), new_url);
  }
  return bookmark_utils::ApplyEditsWithNoGroupChange(
      model, node->GetParent(), BookmarkEditor::EditDetails(node),
      node->GetTitle(), new_url);
}
