// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/bookmarks_helper.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/gfx/image/image_skia.h"

using sync_datatype_helper::test;

namespace {

// History task which runs all pending tasks on the history thread and
// signals when the tasks have completed.
class HistoryEmptyTask : public history::HistoryDBTask {
 public:
  explicit HistoryEmptyTask(base::WaitableEvent* done) : done_(done) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    content::RunAllPendingInMessageLoop();
    done_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}

 private:
  virtual ~HistoryEmptyTask() {}

  base::WaitableEvent* done_;
};

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
    content::RunMessageLoop();
    ASSERT_TRUE(node_->is_favicon_loaded());
    ASSERT_FALSE(model_->GetFavicon(node_).IsEmpty());
  }
  void WaitForSetFavicon() {
    wait_for_load_ = false;
    content::RunMessageLoop();
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
  virtual void BookmarkAllNodesRemoved(BookmarkModel* model) OVERRIDE {}

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

// Represents a favicon image and the icon URL associated with it.
struct FaviconData {
  FaviconData() {
  }

  FaviconData(const gfx::Image& favicon_image,
              const GURL& favicon_url)
      : image(favicon_image),
        icon_url(favicon_url) {
  }

  ~FaviconData() {
  }

  gfx::Image image;
  GURL icon_url;
};

// Gets the favicon and icon URL associated with |node| in |model|.
FaviconData GetFaviconData(BookmarkModel* model,
                           const BookmarkNode* node) {
  // If a favicon wasn't explicitly set for a particular URL, simply return its
  // blank favicon.
  if (!urls_with_favicons_ ||
      urls_with_favicons_->find(node->url()) == urls_with_favicons_->end()) {
    return FaviconData();
  }
  // If a favicon was explicitly set, we may need to wait for it to be loaded
  // via BookmarkModel::GetFavicon(), which is an asynchronous operation.
  if (!node->is_favicon_loaded()) {
    FaviconChangeObserver observer(model, node);
    model->GetFavicon(node);
    observer.WaitForGetFavicon();
  }
  EXPECT_TRUE(node->is_favicon_loaded());
  EXPECT_FALSE(model->GetFavicon(node).IsEmpty());
  return FaviconData(model->GetFavicon(node), node->icon_url());
}

// Sets the favicon for |profile| and |node|. |profile| may be
// |test()->verifier()|.
void SetFaviconImpl(Profile* profile,
                    const BookmarkNode* node,
                    const GURL& icon_url,
                    const gfx::Image& image,
                    bookmarks_helper::FaviconSource favicon_source) {
    BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);

    FaviconChangeObserver observer(model, node);
    FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(profile,
                                             Profile::EXPLICIT_ACCESS);
    if (favicon_source == bookmarks_helper::FROM_UI) {
      favicon_service->SetFavicons(node->url(),
                                   icon_url,
                                   history::FAVICON,
                                   image);
    } else {
      browser_sync::BookmarkChangeProcessor::ApplyBookmarkFavicon(
          node, profile, icon_url, image.As1xPNGBytes());
    }

    // Wait for the favicon for |node| to be invalidated.
    observer.WaitForSetFavicon();
    // Wait for the BookmarkModel to fetch the updated favicon and for the new
    // favicon to be sent to BookmarkChangeProcessor.
    GetFaviconData(model, node);
}

// Wait for all currently scheduled tasks on the history thread for all
// profiles to complete and any notifications sent to the UI thread to have
// finished processing.
void WaitForHistoryToProcessPendingTasks() {
  // Skip waiting for history to complete for tests without favicons.
  if (!urls_with_favicons_)
    return;

  std::vector<Profile*> profiles_which_need_to_wait;
  if (test()->use_verifier())
    profiles_which_need_to_wait.push_back(test()->verifier());
  for (int i = 0; i < test()->num_clients(); ++i)
    profiles_which_need_to_wait.push_back(test()->GetProfile(i));

  for (size_t i = 0; i < profiles_which_need_to_wait.size(); ++i) {
    Profile* profile = profiles_which_need_to_wait[i];
    HistoryService* history_service =
        HistoryServiceFactory::GetForProfileWithoutCreating(profile);
    base::WaitableEvent done(false, false);
    CancelableRequestConsumer request_consumer;
    history_service->ScheduleDBTask(new HistoryEmptyTask(&done),
        &request_consumer);
    done.Wait();
  }
  // Wait such that any notifications broadcast from one of the history threads
  // to the UI thread are processed.
  content::RunAllPendingInMessageLoop();
}

// Checks if the favicon in |node_a| from |model_a| matches that of |node_b|
// from |model_b|. Returns true if they match.
bool FaviconsMatch(BookmarkModel* model_a,
                   BookmarkModel* model_b,
                   const BookmarkNode* node_a,
                   const BookmarkNode* node_b) {
  FaviconData favicon_data_a = GetFaviconData(model_a, node_a);
  FaviconData favicon_data_b = GetFaviconData(model_b, node_b);

  if (favicon_data_a.icon_url != favicon_data_b.icon_url)
    return false;

  gfx::Image image_a = favicon_data_a.image;
  gfx::Image image_b = favicon_data_b.image;

  if (image_a.IsEmpty() && image_b.IsEmpty())
    return true;  // Two empty images are equivalent.

  if (image_a.IsEmpty() != image_b.IsEmpty())
    return false;

  // Compare only the 1x bitmaps as only those are synced.
  SkBitmap bitmap_a = image_a.AsImageSkia().GetRepresentation(
      ui::SCALE_FACTOR_100P).sk_bitmap();
  SkBitmap bitmap_b = image_b.AsImageSkia().GetRepresentation(
      ui::SCALE_FACTOR_100P).sk_bitmap();
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
  return BookmarkModelFactory::GetForProfile(test()->GetProfile(index));
}

const BookmarkNode* GetBookmarkBarNode(int index) {
  return GetBookmarkModel(index)->bookmark_bar_node();
}

const BookmarkNode* GetOtherNode(int index) {
  return GetBookmarkModel(index)->other_node();
}

const BookmarkNode* GetSyncedBookmarksNode(int index) {
  return GetBookmarkModel(index)->mobile_node();
}

BookmarkModel* GetVerifierBookmarkModel() {
  return BookmarkModelFactory::GetForProfile(test()->verifier());
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
                const GURL& icon_url,
                const gfx::Image& image,
                FaviconSource favicon_source) {
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
    SetFaviconImpl(test()->verifier(), v_node, icon_url, image, favicon_source);
  }
  SetFaviconImpl(test()->GetProfile(profile), node, icon_url, image,
                 favicon_source);
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

void RemoveAll(int profile) {
  if (test()->use_verifier()) {
    const BookmarkNode* root_node = GetVerifierBookmarkModel()->root_node();
    for (int i = 0; i < root_node->child_count(); ++i) {
      const BookmarkNode* permanent_node = root_node->GetChild(i);
      for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
        GetVerifierBookmarkModel()->Remove(permanent_node, j);
      }
    }
  }
  GetBookmarkModel(profile)->RemoveAll();
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
  // Ensure that all tasks have finished processing on the history thread
  // and that any notifications the history thread may have sent have been
  // processed before comparing models.
  WaitForHistoryToProcessPendingTasks();

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
  // Ensure that all tasks have finished processing on the history thread
  // and that any notifications the history thread may have sent have been
  // processed before comparing models.
  WaitForHistoryToProcessPendingTasks();

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

gfx::Image CreateFavicon(SkColor color) {
  const int dip_width = 16;
  const int dip_height = 16;
  std::vector<ui::ScaleFactor> favicon_scale_factors =
      FaviconUtil::GetFaviconScaleFactors();
  gfx::ImageSkia favicon;
  for (size_t i = 0; i < favicon_scale_factors.size(); ++i) {
    float scale = ui::GetScaleFactorScale(favicon_scale_factors[i]);
    int pixel_width = dip_width * scale;
    int pixel_height = dip_height * scale;
    SkBitmap bmp;
    bmp.setConfig(SkBitmap::kARGB_8888_Config, pixel_width, pixel_height);
    bmp.allocPixels();
    bmp.eraseColor(color);
    favicon.AddRepresentation(gfx::ImageSkiaRep(bmp, favicon_scale_factors[i]));
  }
  return gfx::Image(favicon);
}

gfx::Image Create1xFaviconFromPNGFile(const std::string& path) {
  const char* kPNGExtension = ".png";
  if (!EndsWith(path, kPNGExtension, false))
    return gfx::Image();

  base::FilePath full_path;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &full_path))
    return gfx::Image();

  full_path = full_path.AppendASCII("sync").AppendASCII(path);
  std::string contents;
  file_util::ReadFileToString(full_path, &contents);
  return gfx::Image::CreateFrom1xPNGBytes(
      reinterpret_cast<const unsigned char*>(contents.data()), contents.size());
}

std::string IndexedURL(int i) {
  return base::StringPrintf("http://www.host.ext:1234/path/filename/%d", i);
}

std::wstring IndexedURLTitle(int i) {
  return base::StringPrintf(L"URL Title %d", i);
}

std::wstring IndexedFolderName(int i) {
  return base::StringPrintf(L"Folder Name %d", i);
}

std::wstring IndexedSubfolderName(int i) {
  return base::StringPrintf(L"Subfolder Name %d", i);
}

std::wstring IndexedSubsubfolderName(int i) {
  return base::StringPrintf(L"Subsubfolder Name %d", i);
}

}  // namespace bookmarks_helper
