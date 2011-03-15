// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/bookmark_model_verifier.h"

#include <stack>
#include <vector>

#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_iterator.h"

namespace {

// Helper class used to wait for the favicon of a particular bookmark node in
// a particular bookmark model to load.
class FaviconLoadObserver : public BookmarkModelObserver {
 public:
  FaviconLoadObserver(BookmarkModel* model, const BookmarkNode* node)
      : model_(model),
        node_(node) {
    model->AddObserver(this);
  }
  virtual ~FaviconLoadObserver() {
    model_->RemoveObserver(this);
  }
  void WaitForFaviconLoad() { ui_test_utils::RunMessageLoop(); }
  virtual void Loaded(BookmarkModel* model) {}
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {
    if (model == model_ && node == node_)
      model->GetFavicon(node);
  }
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {
    if (model == model_ && node == node_)
      MessageLoopForUI::current()->Quit();
  }

 private:
  BookmarkModel* model_;
  const BookmarkNode* node_;
  DISALLOW_COPY_AND_ASSIGN(FaviconLoadObserver);
};

}

// static
bool BookmarkModelVerifier::NodesMatch(const BookmarkNode* node_a,
                                       const BookmarkNode* node_b) {
  if (node_a == NULL || node_b == NULL)
    return node_a == node_b;
  if (node_a->is_folder() != node_b->is_folder()) {
    LOG(ERROR) << "Cannot compare folder with bookmark";
    return false;
  }
  if (node_a->GetTitle() != node_b->GetTitle()) {
    LOG(ERROR) << "Title mismatch: " << node_a->GetTitle() << " vs. "
               << node_b->GetTitle();
    return false;
  }
  if (node_a->GetURL() != node_b->GetURL()) {
    LOG(ERROR) << "URL mismatch: " << node_a->GetURL() << " vs. "
               << node_b->GetURL();
    return false;
  }
  if (node_a->parent()->GetIndexOf(node_a) !=
      node_b->parent()->GetIndexOf(node_b)) {
    LOG(ERROR) << "Index mismatch: "
               << node_a->parent()->GetIndexOf(node_a) << " vs. "
               << node_b->parent()->GetIndexOf(node_b);
    return false;
  }
  return true;
}

// static
bool BookmarkModelVerifier::ModelsMatch(BookmarkModel* model_a,
                                        BookmarkModel* model_b) {
  bool ret_val = true;
  ui::TreeNodeIterator<const BookmarkNode> iterator_a(model_a->root_node());
  ui::TreeNodeIterator<const BookmarkNode> iterator_b(model_b->root_node());
  while (iterator_a.has_next()) {
    const BookmarkNode* node_a = iterator_a.Next();
    EXPECT_TRUE(iterator_b.has_next());
    const BookmarkNode* node_b = iterator_b.Next();
    ret_val = ret_val && NodesMatch(node_a, node_b);
    const SkBitmap& bitmap_a = model_a->GetFavicon(node_a);
    const SkBitmap& bitmap_b = model_b->GetFavicon(node_b);
    ret_val = ret_val && FaviconsMatch(bitmap_a, bitmap_b);
  }
  ret_val = ret_val && (!iterator_b.has_next());
  return ret_val;
}

bool BookmarkModelVerifier::FaviconsMatch(const SkBitmap& bitmap_a,
                                          const SkBitmap& bitmap_b) {
  if (bitmap_a.getSize() == 0U && bitmap_a.getSize() == 0U)
    return true;
  if ((bitmap_a.getSize() != bitmap_b.getSize()) ||
      (bitmap_a.width() != bitmap_b.width()) ||
      (bitmap_a.height() != bitmap_b.height())) {
    LOG(ERROR) << "Favicon size mismatch: " << bitmap_a.getSize() << " ("
               << bitmap_a.width() << "x" << bitmap_a.height() << ") vs. "
               << bitmap_b.getSize() << " (" << bitmap_b.width() << "x"
               << bitmap_b.height() << ")";
    return false;
  }
  SkAutoLockPixels bitmap_lock_a(bitmap_a);
  SkAutoLockPixels bitmap_lock_b(bitmap_b);
  void* node_pixel_addr_a = bitmap_a.getPixels();
  EXPECT_TRUE(node_pixel_addr_a);
  void* node_pixel_addr_b = bitmap_b.getPixels();
  EXPECT_TRUE(node_pixel_addr_b);
  if (memcmp(node_pixel_addr_a, node_pixel_addr_b, bitmap_a.getSize()) !=  0) {
    LOG(ERROR) << "Favicon bitmap mismatch";
    return false;
  } else {
    return true;
  }
}

bool BookmarkModelVerifier::ContainsDuplicateBookmarks(BookmarkModel* model) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
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
          node->parent() == (*it)->parent() &&
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
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
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
    path.push(walker->parent()->GetIndexOf(walker));
    walker = walker->parent();
  }

  // Swing over to the other tree.
  walker = verifier_model_->root_node();

  // Climb down.
  while (!path.empty()) {
    ASSERT_TRUE(walker->is_folder());
    ASSERT_LT(path.top(), walker->child_count());
    walker = walker->GetChild(path.top());
    path.pop();
  }

  ASSERT_TRUE(NodesMatch(foreign_node, walker));
  *result = walker;
}

const BookmarkNode* BookmarkModelVerifier::AddGroup(BookmarkModel* model,
                                                    const BookmarkNode* parent,
                                                    int index,
                                                    const string16& title) {
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

void BookmarkModelVerifier::SetFavicon(
    BookmarkModel* model,
    const BookmarkNode* node,
    const std::vector<unsigned char>& icon_bytes_vector) {
  if (use_verifier_model_) {
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(model, node, &v_node);
    FaviconLoadObserver v_observer(verifier_model_, v_node);
    browser_sync::BookmarkChangeProcessor::ApplyBookmarkFavicon(
        v_node, verifier_model_->profile(), icon_bytes_vector);
    v_observer.WaitForFaviconLoad();
  }
  FaviconLoadObserver observer(model, node);
  browser_sync::BookmarkChangeProcessor::ApplyBookmarkFavicon(
      node, model->profile(), icon_bytes_vector);
  observer.WaitForFaviconLoad();
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
  int child_count = parent->child_count();
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
        verifier_model_, v_node->parent(),
        BookmarkEditor::EditDetails(v_node), v_node->GetTitle(), new_url);
  }
  return bookmark_utils::ApplyEditsWithNoGroupChange(
      model, node->parent(), BookmarkEditor::EditDetails(node),
      node->GetTitle(), new_url);
}
