// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_model_associator.h"

#include <stack>

#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

// The sync protocol identifies top-level entities by means of well-known tags,
// which should not be confused with titles.  Each tag corresponds to a
// singleton instance of a particular top-level node in a user's share; the
// tags are consistent across users. The tags allow us to locate the specific
// folders whose contents we care about synchronizing, without having to do a
// lookup by name or path.  The tags should not be made user-visible.
// For example, the tag "bookmark_bar" represents the permanent node for
// bookmarks bar in Chrome. The tag "other_bookmarks" represents the permanent
// folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing,
// the sync server) to create these tagged nodes when initializing sync
// for the first time for a user.  Thus, once the backend finishes
// initializing, the ProfileSyncService can rely on the presence of tagged
// nodes.
//
// TODO(ncarter): Pull these tags from an external protocol specification
// rather than hardcoding them here.
static const char kBookmarkBarTag[] = "bookmark_bar";
static const char kMobileBookmarksTag[] = "synced_bookmarks";
static const char kOtherBookmarksTag[] = "other_bookmarks";
static const char kServerError[] =
    "Server did not create top-level nodes.  Possibly we are running against "
    "an out-of-date server?";

// Bookmark comparer for map of bookmark nodes.
class BookmarkComparer {
 public:
  // Compares the two given nodes and returns whether node1 should appear
  // before node2 in strict weak ordering.
  bool operator()(const BookmarkNode* node1,
                  const BookmarkNode* node2) const {
    DCHECK(node1);
    DCHECK(node2);

    // Keep folder nodes before non-folder nodes.
    if (node1->is_folder() != node2->is_folder())
      return node1->is_folder();

    int result = node1->GetTitle().compare(node2->GetTitle());
    if (result != 0)
      return result < 0;

    return node1->url() < node2->url();
  }
};

// Provides the following abstraction: given a parent bookmark node, find best
// matching child node for many sync nodes.
class BookmarkNodeFinder {
 public:
  // Creates an instance with the given parent bookmark node.
  explicit BookmarkNodeFinder(const BookmarkNode* parent_node);

  // Finds best matching node for the given sync node.
  // Returns the matching node if one exists; NULL otherwise. If a matching
  // node is found, it's removed for further matches.
  const BookmarkNode* FindBookmarkNode(const sync_api::BaseNode& sync_node);

 private:
  typedef std::multiset<const BookmarkNode*, BookmarkComparer> BookmarkNodesSet;

  const BookmarkNode* parent_node_;
  BookmarkNodesSet child_nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeFinder);
};

BookmarkNodeFinder::BookmarkNodeFinder(const BookmarkNode* parent_node)
    : parent_node_(parent_node) {
  for (int i = 0; i < parent_node_->child_count(); ++i) {
    child_nodes_.insert(parent_node_->GetChild(i));
  }
}

const BookmarkNode* BookmarkNodeFinder::FindBookmarkNode(
    const sync_api::BaseNode& sync_node) {
  // Create a bookmark node from the given sync node.
  BookmarkNode temp_node(sync_node.GetURL());
  temp_node.set_title(UTF8ToUTF16(sync_node.GetTitle()));
  if (sync_node.GetIsFolder())
    temp_node.set_type(BookmarkNode::FOLDER);
  else
    temp_node.set_type(BookmarkNode::URL);

  const BookmarkNode* result = NULL;
  BookmarkNodesSet::iterator iter = child_nodes_.find(&temp_node);
  if (iter != child_nodes_.end()) {
    result = *iter;
    // Remove the matched node so we don't match with it again.
    child_nodes_.erase(iter);
  }

  return result;
}

// Helper class to build an index of bookmark nodes by their IDs.
class BookmarkNodeIdIndex {
 public:
  BookmarkNodeIdIndex() { }
  ~BookmarkNodeIdIndex() { }

  // Adds the given bookmark node and all its descendants to the ID index.
  // Does nothing if node is NULL.
  void AddAll(const BookmarkNode* node);

  // Finds the bookmark node with the given ID.
  // Returns NULL if none exists with the given id.
  const BookmarkNode* Find(int64 id) const;

  // Returns the count of nodes in the index.
  size_t count() const { return node_index_.size(); }

 private:
  typedef base::hash_map<int64, const BookmarkNode*> BookmarkIdMap;
  // Map that holds nodes indexed by their ids.
  BookmarkIdMap node_index_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeIdIndex);
};

void BookmarkNodeIdIndex::AddAll(const BookmarkNode* node) {
  if (!node)
    return;

  node_index_[node->id()] = node;

  if (!node->is_folder())
    return;

  for (int i = 0; i < node->child_count(); ++i)
    AddAll(node->GetChild(i));
}

const BookmarkNode* BookmarkNodeIdIndex::Find(int64 id) const {
  BookmarkIdMap::const_iterator iter = node_index_.find(id);
  return iter == node_index_.end() ? NULL : iter->second;
}

BookmarkModelAssociator::BookmarkModelAssociator(
    BookmarkModel* bookmark_model,
    sync_api::UserShare* user_share,
    UnrecoverableErrorHandler* unrecoverable_error_handler)
    : bookmark_model_(bookmark_model),
      user_share_(user_share),
      unrecoverable_error_handler_(unrecoverable_error_handler),
      ALLOW_THIS_IN_INITIALIZER_LIST(persist_associations_(this)),
      number_of_new_sync_nodes_created_at_association_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(bookmark_model_);
  DCHECK(user_share_);
  DCHECK(unrecoverable_error_handler_);
}

BookmarkModelAssociator::~BookmarkModelAssociator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void BookmarkModelAssociator::UpdateMobileNodeVisibility() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(bookmark_model_->IsLoaded());

  bookmark_model_->SetMobileFolderVisible(
      id_map_.find(bookmark_model_->mobile_node()->id()) != id_map_.end());
}

bool BookmarkModelAssociator::DisassociateModels(SyncError* error) {
  id_map_.clear();
  id_map_inverse_.clear();
  dirty_associations_sync_ids_.clear();
  return true;
}

int64 BookmarkModelAssociator::GetSyncIdFromChromeId(const int64& node_id) {
  BookmarkIdToSyncIdMap::const_iterator iter = id_map_.find(node_id);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

const BookmarkNode* BookmarkModelAssociator::GetChromeNodeFromSyncId(
    int64 sync_id) {
  SyncIdToBookmarkNodeMap::const_iterator iter = id_map_inverse_.find(sync_id);
  return iter == id_map_inverse_.end() ? NULL : iter->second;
}

bool BookmarkModelAssociator::InitSyncNodeFromChromeId(
    const int64& node_id,
    sync_api::BaseNode* sync_node) {
  DCHECK(sync_node);
  int64 sync_id = GetSyncIdFromChromeId(node_id);
  if (sync_id == sync_api::kInvalidId)
    return false;
  if (!sync_node->InitByIdLookup(sync_id))
    return false;
  DCHECK(sync_node->GetId() == sync_id);
  return true;
}

void BookmarkModelAssociator::Associate(const BookmarkNode* node,
                                        int64 sync_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int64 node_id = node->id();
  DCHECK_NE(sync_id, sync_api::kInvalidId);
  DCHECK(id_map_.find(node_id) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[node_id] = sync_id;
  id_map_inverse_[sync_id] = node;
  dirty_associations_sync_ids_.insert(sync_id);
  PostPersistAssociationsTask();
  UpdateMobileNodeVisibility();
}

void BookmarkModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SyncIdToBookmarkNodeMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second->id());
  id_map_inverse_.erase(iter);
  dirty_associations_sync_ids_.erase(sync_id);
}

bool BookmarkModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  bool has_mobile_folder = true;
  int64 bookmark_bar_sync_id;
  if (!GetSyncIdForTaggedNode(kBookmarkBarTag, &bookmark_bar_sync_id)) {
    return false;
  }
  int64 other_bookmarks_sync_id;
  if (!GetSyncIdForTaggedNode(kOtherBookmarksTag, &other_bookmarks_sync_id)) {
    return false;
  }
  int64 mobile_bookmarks_sync_id;
  if (!GetSyncIdForTaggedNode(kMobileBookmarksTag, &mobile_bookmarks_sync_id)) {
    has_mobile_folder = false;
  }

  sync_api::ReadTransaction trans(FROM_HERE, user_share_);

  sync_api::ReadNode bookmark_bar_node(&trans);
  if (!bookmark_bar_node.InitByIdLookup(bookmark_bar_sync_id)) {
    return false;
  }

  sync_api::ReadNode other_bookmarks_node(&trans);
  if (!other_bookmarks_node.InitByIdLookup(other_bookmarks_sync_id)) {
    return false;
  }

  sync_api::ReadNode mobile_bookmarks_node(&trans);
  if (has_mobile_folder &&
      !mobile_bookmarks_node.InitByIdLookup(mobile_bookmarks_sync_id)) {
    return false;
  }

  // Sync model has user created nodes if any of the permanent nodes has
  // children.
  *has_nodes = bookmark_bar_node.HasChildren() ||
      other_bookmarks_node.HasChildren() ||
      (has_mobile_folder && mobile_bookmarks_node.HasChildren());
  return true;
}

bool BookmarkModelAssociator::NodesMatch(
    const BookmarkNode* bookmark,
    const sync_api::BaseNode* sync_node) const {
  if (bookmark->GetTitle() != UTF8ToUTF16(sync_node->GetTitle()))
    return false;
  if (bookmark->is_folder() != sync_node->GetIsFolder())
    return false;
  if (bookmark->is_url()) {
    if (bookmark->url() != sync_node->GetURL())
      return false;
  }
  // Don't compare favicons here, because they are not really
  // user-updated and we don't have versioning information -- a site changing
  // its favicon shouldn't result in a bookmark mismatch.
  return true;
}

bool BookmarkModelAssociator::AssociateTaggedPermanentNode(
    const BookmarkNode* permanent_node, const std::string&tag) {
  // Do nothing if |permanent_node| is already initialized and associated.
  int64 sync_id = GetSyncIdFromChromeId(permanent_node->id());
  if (sync_id != sync_api::kInvalidId)
    return true;
  if (!GetSyncIdForTaggedNode(tag, &sync_id))
    return false;

  Associate(permanent_node, sync_id);
  return true;
}

bool BookmarkModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                     int64* sync_id) {
  sync_api::ReadTransaction trans(FROM_HERE, user_share_);
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

bool BookmarkModelAssociator::AssociateModels(SyncError* error) {
  // Try to load model associations from persisted associations first. If that
  // succeeds, we don't need to run the complex model matching algorithm.
  if (LoadAssociations())
    return true;

  DisassociateModels(error);

  // We couldn't load model associations from persisted associations. So build
  // them.
  return BuildAssociations(error);
}

bool BookmarkModelAssociator::BuildAssociations(SyncError* error) {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each sync node for the current sync parent node, find the best
  //   matching bookmark node under the corresponding bookmark parent node.
  //   If no matching node is found, create a new bookmark node in the same
  //   position as the corresponding sync node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding sync node.
  // * When all children sync nodes are done, add the extra children bookmark
  //   nodes to the sync parent node.
  //
  // This algorithm will do a good job of merging when folder names are a good
  // indicator of the two folders being the same. It will handle reordering and
  // new node addition very well (without creating duplicates).
  // This algorithm will not do well if the folder name has changes but the
  // children under them are all the same.

  DCHECK(bookmark_model_->IsLoaded());

  // To prime our association, we associate the top-level nodes, Bookmark Bar
  // and Other Bookmarks.
  if (!AssociateTaggedPermanentNode(bookmark_model_->other_node(),
                                    kOtherBookmarksTag)) {
    error->Reset(FROM_HERE, kServerError, model_type());
    return false;
  }
  if (!AssociateTaggedPermanentNode(bookmark_model_->bookmark_bar_node(),
                                    kBookmarkBarTag)) {
    error->Reset(FROM_HERE, kServerError, model_type());
    return false;
  }
  if (!AssociateTaggedPermanentNode(bookmark_model_->mobile_node(),
                                    kMobileBookmarksTag) &&
      // The mobile folder only need exist if kCreateMobileBookmarksFolder is
      // set.
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCreateMobileBookmarksFolder)) {
    error->Reset(FROM_HERE, kServerError, model_type());
    return false;
  }
  int64 bookmark_bar_sync_id = GetSyncIdFromChromeId(
      bookmark_model_->bookmark_bar_node()->id());
  DCHECK_NE(bookmark_bar_sync_id, sync_api::kInvalidId);
  int64 other_bookmarks_sync_id = GetSyncIdFromChromeId(
      bookmark_model_->other_node()->id());
  DCHECK_NE(other_bookmarks_sync_id, sync_api::kInvalidId);
  int64 mobile_bookmarks_sync_id = GetSyncIdFromChromeId(
       bookmark_model_->mobile_node()->id());
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCreateMobileBookmarksFolder)) {
    DCHECK_NE(sync_api::kInvalidId, mobile_bookmarks_sync_id);
  }

  std::stack<int64> dfs_stack;
  if (mobile_bookmarks_sync_id != sync_api::kInvalidId)
    dfs_stack.push(mobile_bookmarks_sync_id);
  dfs_stack.push(other_bookmarks_sync_id);
  dfs_stack.push(bookmark_bar_sync_id);

  sync_api::WriteTransaction trans(FROM_HERE, user_share_);

  while (!dfs_stack.empty()) {
    int64 sync_parent_id = dfs_stack.top();
    dfs_stack.pop();

    sync_api::ReadNode sync_parent(&trans);
    if (!sync_parent.InitByIdLookup(sync_parent_id)) {
      error->Reset(FROM_HERE, "Failed to lookup node.", model_type());
      return false;
    }
    // Only folder nodes are pushed on to the stack.
    DCHECK(sync_parent.GetIsFolder());

    const BookmarkNode* parent_node = GetChromeNodeFromSyncId(sync_parent_id);
    DCHECK(parent_node->is_folder());

    BookmarkNodeFinder node_finder(parent_node);

    int index = 0;
    int64 sync_child_id = sync_parent.GetFirstChildId();
    while (sync_child_id != sync_api::kInvalidId) {
      sync_api::WriteNode sync_child_node(&trans);
      if (!sync_child_node.InitByIdLookup(sync_child_id)) {
        error->Reset(FROM_HERE, "Failed to lookup node.", model_type());
        return false;
      }

      const BookmarkNode* child_node = NULL;
      child_node = node_finder.FindBookmarkNode(sync_child_node);
      if (child_node) {
        bookmark_model_->Move(child_node, parent_node, index);
        // Set the favicon for bookmark node from sync node or vice versa.
        if (BookmarkChangeProcessor::SetBookmarkFavicon(
            &sync_child_node, child_node, bookmark_model_)) {
          BookmarkChangeProcessor::SetSyncNodeFavicon(
              child_node, bookmark_model_, &sync_child_node);
        }
      } else {
        // Create a new bookmark node for the sync node.
        child_node = BookmarkChangeProcessor::CreateBookmarkNode(
            &sync_child_node, parent_node, bookmark_model_, index);
      }
      Associate(child_node, sync_child_id);
      if (sync_child_node.GetIsFolder())
        dfs_stack.push(sync_child_id);

      sync_child_id = sync_child_node.GetSuccessorId();
      ++index;
    }

    // At this point all the children nodes of the parent sync node have
    // corresponding children in the parent bookmark node and they are all in
    // the right positions: from 0 to index - 1.
    // So the children starting from index in the parent bookmark node are the
    // ones that are not present in the parent sync node. So create them.
    for (int i = index; i < parent_node->child_count(); ++i) {
      sync_child_id = BookmarkChangeProcessor::CreateSyncNode(
          parent_node, bookmark_model_, i, &trans, this,
          unrecoverable_error_handler_);
      if (sync_api::kInvalidId == sync_child_id) {
        error->Reset(FROM_HERE, "Failed to create sync node.", model_type());
        return false; // Creation failed.
      }
      if (parent_node->GetChild(i)->is_folder())
        dfs_stack.push(sync_child_id);
      number_of_new_sync_nodes_created_at_association_++;
    }
  }

  return true;
}

void BookmarkModelAssociator::PostPersistAssociationsTask() {
  // No need to post a task if a task is already pending.
  if (!persist_associations_.empty())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      persist_associations_.NewRunnableMethod(
          &BookmarkModelAssociator::PersistAssociations));
}

void BookmarkModelAssociator::PersistAssociations() {
  // If there are no dirty associations we have nothing to do. We handle this
  // explicity instead of letting the for loop do it to avoid creating a write
  // transaction in this case.
  if (dirty_associations_sync_ids_.empty()) {
    DCHECK(id_map_.empty());
    DCHECK(id_map_inverse_.empty());
    return;
  }

  sync_api::WriteTransaction trans(FROM_HERE, user_share_);
  DirtyAssociationsSyncIds::iterator iter;
  for (iter = dirty_associations_sync_ids_.begin();
       iter != dirty_associations_sync_ids_.end();
       ++iter) {
    int64 sync_id = *iter;
    sync_api::WriteNode sync_node(&trans);
    if (!sync_node.InitByIdLookup(sync_id)) {
      unrecoverable_error_handler_->OnUnrecoverableError(FROM_HERE,
          "Could not lookup bookmark node for ID persistence.");
      return;
    }
    const BookmarkNode* node = GetChromeNodeFromSyncId(sync_id);
    if (node)
      sync_node.SetExternalId(node->id());
    else
      NOTREACHED();
  }
  dirty_associations_sync_ids_.clear();
}

bool BookmarkModelAssociator::LoadAssociations() {
  DCHECK(bookmark_model_->IsLoaded());
  // If the bookmarks changed externally, our previous associations may not be
  // valid; so return false.
  if (bookmark_model_->file_changed())
    return false;

  // Our persisted associations should be valid. Try to populate id association
  // maps using persisted associations.  Note that the unit tests will
  // create the tagged nodes on demand, and the order in which we probe for
  // them here will impact their positional ordering in that case.
  int64 bookmark_bar_id;
  if (!GetSyncIdForTaggedNode(kBookmarkBarTag, &bookmark_bar_id)) {
    // We should always be able to find the permanent nodes.
    return false;
  }
  int64 other_bookmarks_id;
  if (!GetSyncIdForTaggedNode(kOtherBookmarksTag, &other_bookmarks_id)) {
    // We should always be able to find the permanent nodes.
    return false;
  }
  int64 mobile_bookmarks_id = -1;
  if (!GetSyncIdForTaggedNode(kMobileBookmarksTag, &mobile_bookmarks_id) &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCreateMobileBookmarksFolder)) {
    return false;
  }

  // Build a bookmark node ID index since we are going to repeatedly search for
  // bookmark nodes by their IDs.
  BookmarkNodeIdIndex id_index;
  id_index.AddAll(bookmark_model_->bookmark_bar_node());
  id_index.AddAll(bookmark_model_->other_node());
  id_index.AddAll(bookmark_model_->mobile_node());

  std::stack<int64> dfs_stack;
  if (mobile_bookmarks_id != -1)
    dfs_stack.push(mobile_bookmarks_id);
  dfs_stack.push(other_bookmarks_id);
  dfs_stack.push(bookmark_bar_id);

  sync_api::ReadTransaction trans(FROM_HERE, user_share_);

  // Count total number of nodes in sync model so that we can compare that
  // with the total number of nodes in the bookmark model.
  size_t sync_node_count = 0;
  while (!dfs_stack.empty()) {
    int64 parent_id = dfs_stack.top();
    dfs_stack.pop();
    ++sync_node_count;
    sync_api::ReadNode sync_parent(&trans);
    if (!sync_parent.InitByIdLookup(parent_id)) {
      return false;
    }

    int64 external_id = sync_parent.GetExternalId();
    if (external_id == 0)
      return false;

    const BookmarkNode* node = id_index.Find(external_id);
    if (!node)
      return false;

    // Don't try to call NodesMatch on permanent nodes like bookmark bar and
    // other bookmarks. They are not expected to match.
    if (node != bookmark_model_->bookmark_bar_node() &&
        node != bookmark_model_->mobile_node() &&
        node != bookmark_model_->other_node() &&
        !NodesMatch(node, &sync_parent))
      return false;

    Associate(node, sync_parent.GetId());

    // Add all children of the current node to the stack.
    int64 child_id = sync_parent.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      dfs_stack.push(child_id);
      sync_api::ReadNode child_node(&trans);
      if (!child_node.InitByIdLookup(child_id)) {
        return false;
      }
      child_id = child_node.GetSuccessorId();
    }
  }
  DCHECK(dfs_stack.empty());

  // It's possible that the number of nodes in the bookmark model is not the
  // same as number of nodes in the sync model. This can happen when the sync
  // model doesn't get a chance to persist its changes, for example when
  // Chrome does not shut down gracefully. In such cases we can't trust the
  // loaded associations.
  return sync_node_count == id_index.count();
}

bool BookmarkModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(FROM_HERE, user_share_);
  const syncable::ModelTypeSet& encrypted_types =
      sync_api::GetEncryptedTypes(&trans);
  return encrypted_types.count(syncable::BOOKMARKS) == 0 ||
      trans.GetCryptographer()->is_ready();
}

}  // namespace browser_sync
