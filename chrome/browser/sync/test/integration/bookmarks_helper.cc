// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/bookmarks_helper.h"

#include "base/compiler_specific.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/gfx/codec/png_codec.h"

using sync_datatype_helper::test;

namespace {

// Helper class used to wait for changes to take effect on the favicon of a
// particular bookmark node in a particular bookmark model.
class FaviconChangeObserver : public BookmarkModelObserver {
 public:
  FaviconChangeObserver(BookmarkModel* model, const BookmarkNode* node)
      : model_(model),
        node_(node),
        wait_for_load_(false) {
    model->AddObserver(this);
  }
  virtual ~FaviconChangeObserver() {
    model_->RemoveObserver(this);
  }
  void WaitForGetFavicon() {
    wait_for_load_ = true;
    ui_test_utils::RunMessageLoop();
    ASSERT_TRUE(node_->is_favicon_loaded());
  }
  void WaitForSetFavicon() {
    wait_for_load_ = false;
    ui_test_utils::RunMessageLoop();
  }
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE {}
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE {
    if (model == model_ && node == node_)
      model->GetFavicon(node);
  }
  virtual void BookmarkNodeChildrenReordered(
      BookmarkModel* model,
      const BookmarkNode* node) OVERRIDE {}
  virtual void BookmarkNodeFaviconChanged(
      BookmarkModel* model,
      const BookmarkNode* node) OVERRIDE {
    if (model == model_ && node == node_) {
      if (!wait_for_load_ || (wait_for_load_ && node->is_favicon_loaded()))
        MessageLoopForUI::current()->Quit();
    }
  }

 private:
  BookmarkModel* model_;
  const BookmarkNode* node_;
  bool wait_for_load_;
  DISALLOW_COPY_AND_ASSIGN(FaviconChangeObserver);
};

// A collection of URLs for which we have added favicons. Since loading a
// favicon is an asynchronous operation and doesn't necessarily invoke a
// callback, this collection is used to determine if we must wait for a URL's
// favicon to load or not.
std::set<GURL>* urls_with_favicons_ = NULL;

// Returns the number of nodes of node type |node_type| in |model| whose
// titles match the string |title|.
int CountNodesWithTitlesMatching(BookmarkModel* model,
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

// Checks if the favicon data in |bitmap_a| and |bitmap_b| are equivalent.
// Returns true if they match.
bool FaviconBitmapsMatch(const SkBitmap& bitmap_a, const SkBitmap& bitmap_b) {
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

// Gets the favicon associated with |node| in |model|.
const SkBitmap& GetFavicon(BookmarkModel* model, const BookmarkNode* node) {
  // If a favicon wasn't explicitly set for a particular URL, simply return its
  // blank favicon.
  if (!urls_with_favicons_ ||
      urls_with_favicons_->find(node->url()) == urls_with_favicons_->end()) {
    return node->favicon();
  }
  // If a favicon was explicitly set, we may need to wait for it to be loaded
  // via BookmarkModel::GetFavIcon(), which is an asynchronous operation.
  if (!node->is_favicon_loaded()) {
    FaviconChangeObserver observer(model, node);
    model->GetFavicon(node);
    observer.WaitForGetFavicon();
  }
  EXPECT_TRUE(node->is_favicon_loaded());
  return node->favicon();
}

// Checks if the favicon in |node_a| from |model_a| matches that of |node_b|
// from |model_b|. Returns true if they match.
bool FaviconsMatch(BookmarkModel* model_a,
                   BookmarkModel* model_b,
                   const BookmarkNode* node_a,
                   const BookmarkNode* node_b) {
  const SkBitmap& bitmap_a = GetFavicon(model_a, node_a);
  const SkBitmap& bitmap_b = GetFavicon(model_b, node_b);
  return FaviconBitmapsMatch(bitmap_a, bitmap_b);
}

// Does a deep comparison of BookmarkNode fields in |model_a| and |model_b|.
// Returns true if they are all equal.
bool NodesMatch(const BookmarkNode* node_a, const BookmarkNode* node_b) {
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
  if (node_a->url() != node_b->url()) {
    LOG(ERROR) << "URL mismatch: " << node_a->url() << " vs. "
               << node_b->url();
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

// Checks if the hierarchies in |model_a| and |model_b| are equivalent in
// terms of the data model and favicon. Returns true if they both match.
// Note: Some peripheral fields like creation times are allowed to mismatch.
bool BookmarkModelsMatch(BookmarkModel* model_a, BookmarkModel* model_b) {
  bool ret_val = true;
  ui::TreeNodeIterator<const BookmarkNode> iterator_a(model_a->root_node());
  ui::TreeNodeIterator<const BookmarkNode> iterator_b(model_b->root_node());
  while (iterator_a.has_next()) {
    const BookmarkNode* node_a = iterator_a.Next();
    if (!iterator_b.has_next()) {
      LOG(ERROR) << "Models do not match.";
      return false;
    }
    const BookmarkNode* node_b = iterator_b.Next();
    ret_val = ret_val && NodesMatch(node_a, node_b);
    if (node_a->is_folder() || node_b->is_folder())
      continue;
    ret_val = ret_val && FaviconsMatch(model_a, model_b, node_a, node_b);
  }
  ret_val = ret_val && (!iterator_b.has_next());
  return ret_val;
}

// Finds the node in the verifier bookmark model that corresponds to
// |foreign_node| in |foreign_model| and stores its address in |result|.
void FindNodeInVerifier(BookmarkModel* foreign_model,
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
  walker = bookmarks_helper::GetVerifierBookmarkModel()->root_node();

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

}  // namespace


namespace bookmarks_helper {

BookmarkModel* GetBookmarkModel(int index) {
  return test()->GetProfile(index)->GetBookmarkModel();
}

const BookmarkNode* GetBookmarkBarNode(int index) {
  return GetBookmarkModel(index)->bookmark_bar_node();
}

const BookmarkNode* GetOtherNode(int index) {
  return GetBookmarkModel(index)->other_node();
}

BookmarkModel* GetVerifierBookmarkModel() {
  return test()->verifier()->GetBookmarkModel();
}

const BookmarkNode* AddURL(int profile,
                           const std::wstring& title,
                           const GURL& url) {
  return AddURL(profile, GetBookmarkBarNode(profile), 0, title,  url);
}

const BookmarkNode* AddURL(int profile,
                           int index,
                           const std::wstring& title,
                           const GURL& url) {
  return AddURL(profile, GetBookmarkBarNode(profile), index, title, url);
}

const BookmarkNode* AddURL(int profile,
                           const BookmarkNode* parent,
                           int index,
                           const std::wstring& title,
                           const GURL& url) {
  if (GetBookmarkModel(profile)->GetNodeByID(parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  const BookmarkNode* result = GetBookmarkModel(profile)->
      AddURL(parent, index, WideToUTF16(title), url);
  if (!result) {
    LOG(ERROR) << "Could not add bookmark " << title << " to Profile "
               << profile;
    return NULL;
  }
  if (test()->use_verifier()) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), parent, &v_parent);
    const BookmarkNode* v_node = GetVerifierBookmarkModel()->
        AddURL(v_parent, index, WideToUTF16(title), url);
    if (!v_node) {
      LOG(ERROR) << "Could not add bookmark " << title << " to the verifier";
      return NULL;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

const BookmarkNode* AddFolder(int profile,
                              const std::wstring& title) {
  return AddFolder(profile, GetBookmarkBarNode(profile), 0, title);
}

const BookmarkNode* AddFolder(int profile,
                              int index,
                              const std::wstring& title) {
  return AddFolder(profile, GetBookmarkBarNode(profile), index, title);
}

const BookmarkNode* AddFolder(int profile,
                              const BookmarkNode* parent,
                              int index,
                              const std::wstring& title) {
  if (GetBookmarkModel(profile)->GetNodeByID(parent->id()) != parent) {
    LOG(ERROR) << "Node " << parent->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  const BookmarkNode* result =
      GetBookmarkModel(profile)->AddFolder(parent, index, WideToUTF16(title));
  EXPECT_TRUE(result);
  if (!result) {
    LOG(ERROR) << "Could not add folder " << title << " to Profile "
               << profile;
    return NULL;
  }
  if (test()->use_verifier()) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), parent, &v_parent);
    const BookmarkNode* v_node = GetVerifierBookmarkModel()->AddFolder(
        v_parent, index, WideToUTF16(title));
    if (!v_node) {
      LOG(ERROR) << "Could not add folder " << title << " to the verifier";
      return NULL;
    }
    EXPECT_TRUE(NodesMatch(v_node, result));
  }
  return result;
}

void SetTitle(int profile,
              const BookmarkNode* node,
              const std::wstring& new_title) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (test()->use_verifier()) {
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), node, &v_node);
    GetVerifierBookmarkModel()->SetTitle(v_node, WideToUTF16(new_title));
  }
  GetBookmarkModel(profile)->SetTitle(node, WideToUTF16(new_title));
}

void SetFavicon(int profile,
                const BookmarkNode* node,
                const std::vector<unsigned char>& icon_bytes_vector) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  ASSERT_EQ(BookmarkNode::URL, node->type())
      << "Node " << node->GetTitle() << " must be a url.";
  if (urls_with_favicons_ == NULL)
    urls_with_favicons_ = new std::set<GURL>();
  urls_with_favicons_->insert(node->url());
  if (test()->use_verifier()) {
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), node, &v_node);
    FaviconChangeObserver v_observer(GetVerifierBookmarkModel(), v_node);
    browser_sync::BookmarkChangeProcessor::ApplyBookmarkFavicon(
        v_node, test()->verifier(), icon_bytes_vector);
    v_observer.WaitForSetFavicon();
  }
  FaviconChangeObserver observer(GetBookmarkModel(profile), node);
  browser_sync::BookmarkChangeProcessor::ApplyBookmarkFavicon(
      node, test()->GetProfile(profile), icon_bytes_vector);
  observer.WaitForSetFavicon();
}

const BookmarkNode* SetURL(int profile,
                           const BookmarkNode* node,
                           const GURL& new_url) {
  if (GetBookmarkModel(profile)->GetNodeByID(node->id()) != node) {
    LOG(ERROR) << "Node " << node->GetTitle() << " does not belong to "
               << "Profile " << profile;
    return NULL;
  }
  if (test()->use_verifier()) {
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), node, &v_node);
    bookmark_utils::ApplyEditsWithNoFolderChange(
        GetVerifierBookmarkModel(),
        v_node->parent(),
        BookmarkEditor::EditDetails::EditNode(v_node),
        v_node->GetTitle(),
        new_url);
  }
  return bookmark_utils::ApplyEditsWithNoFolderChange(
      GetBookmarkModel(profile),
      node->parent(),
      BookmarkEditor::EditDetails::EditNode(node),
      node->GetTitle(),
      new_url);
}

void Move(int profile,
          const BookmarkNode* node,
          const BookmarkNode* new_parent,
          int index) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(node->id()), node)
      << "Node " << node->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (test()->use_verifier()) {
    const BookmarkNode* v_new_parent = NULL;
    const BookmarkNode* v_node = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), new_parent, &v_new_parent);
    FindNodeInVerifier(GetBookmarkModel(profile), node, &v_node);
    GetVerifierBookmarkModel()->Move(v_node, v_new_parent, index);
  }
  GetBookmarkModel(profile)->Move(node, new_parent, index);
}

void Remove(int profile,
            const BookmarkNode* parent,
            int index) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (test()->use_verifier()) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), parent, &v_parent);
    ASSERT_TRUE(NodesMatch(parent->GetChild(index), v_parent->GetChild(index)));
    GetVerifierBookmarkModel()->Remove(v_parent, index);
  }
  GetBookmarkModel(profile)->Remove(parent, index);
}

void SortChildren(int profile, const BookmarkNode* parent) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  if (test()->use_verifier()) {
    const BookmarkNode* v_parent = NULL;
    FindNodeInVerifier(GetBookmarkModel(profile), parent, &v_parent);
    GetVerifierBookmarkModel()->SortChildren(v_parent);
  }
  GetBookmarkModel(profile)->SortChildren(parent);
}

void ReverseChildOrder(int profile, const BookmarkNode* parent) {
  ASSERT_EQ(GetBookmarkModel(profile)->GetNodeByID(parent->id()), parent)
      << "Node " << parent->GetTitle() << " does not belong to "
      << "Profile " << profile;
  int child_count = parent->child_count();
  if (child_count <= 0)
    return;
  for (int index = 0; index < child_count; ++index) {
    Move(profile, parent->GetChild(index), parent, child_count - index);
  }
}

bool ModelMatchesVerifier(int profile) {
  if (!test()->use_verifier()) {
    LOG(ERROR) << "Illegal to call ModelMatchesVerifier() after "
               << "DisableVerifier(). Use ModelsMatch() instead.";
    return false;
  }
  return BookmarkModelsMatch(GetVerifierBookmarkModel(),
                             GetBookmarkModel(profile));
}

bool AllModelsMatchVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!ModelMatchesVerifier(i)) {
      LOG(ERROR) << "Model " << i << " does not match the verifier.";
      return false;
    }
  }
  return true;
}

bool ModelsMatch(int profile_a, int profile_b) {
  return BookmarkModelsMatch(GetBookmarkModel(profile_a),
                             GetBookmarkModel(profile_b));
}

bool AllModelsMatch() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ModelsMatch(0, i)) {
      LOG(ERROR) << "Model " << i << " does not match Model 0.";
      return false;
    }
  }
  return true;
}

bool ContainsDuplicateBookmarks(int profile) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      GetBookmarkModel(profile)->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_folder())
      continue;
    std::vector<const BookmarkNode*> nodes;
    GetBookmarkModel(profile)->GetNodesByURL(node->url(), &nodes);
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

bool HasNodeWithURL(int profile, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
  return !nodes.empty();
}

const BookmarkNode* GetUniqueNodeByURL(int profile, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarkModel(profile)->GetNodesByURL(url, &nodes);
  EXPECT_EQ(1U, nodes.size());
  if (nodes.empty())
    return NULL;
  return nodes[0];
}

int CountBookmarksWithTitlesMatching(int profile, const std::wstring& title) {
  return CountNodesWithTitlesMatching(GetBookmarkModel(profile),
                                      BookmarkNode::URL,
                                      WideToUTF16(title));
}

int CountFoldersWithTitlesMatching(int profile, const std::wstring& title) {
  return CountNodesWithTitlesMatching(GetBookmarkModel(profile),
                                      BookmarkNode::FOLDER,
                                      WideToUTF16(title));
}

std::vector<unsigned char> CreateFavicon(int seed) {
  const int w = 16;
  const int h = 16;
  SkBitmap bmp;
  bmp.setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp.allocPixels();
  uint32_t* src_data = bmp.getAddr32(0, 0);
  for (int i = 0; i < w * h; ++i) {
    src_data[i] = SkPreMultiplyARGB((seed + i) % 255,
                                    (seed + i) % 250,
                                    (seed + i) % 245,
                                    (seed + i) % 240);
  }
  std::vector<unsigned char> favicon;
  gfx::PNGCodec::EncodeBGRASkBitmap(bmp, false, &favicon);
  return favicon;
}

std::string IndexedURL(int i) {
  return StringPrintf("http://www.host.ext:1234/path/filename/%d", i);
}

std::wstring IndexedURLTitle(int i) {
  return StringPrintf(L"URL Title %d", i);
}

std::wstring IndexedFolderName(int i) {
  return StringPrintf(L"Folder Name %d", i);
}

std::wstring IndexedSubfolderName(int i) {
  return StringPrintf(L"Subfolder Name %d", i);
}

std::wstring IndexedSubsubfolderName(int i) {
  return StringPrintf(L"Subsubfolder Name %d", i);
}

}  // namespace bookmarks_helper
