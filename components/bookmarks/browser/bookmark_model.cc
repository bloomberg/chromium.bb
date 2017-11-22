// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/string_compare.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/bookmarks/browser/bookmark_expanded_state_tracker.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/bookmark_undo_delegate.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/typed_count_sorter.h"
#include "components/favicon_base/favicon_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

using base::Time;

namespace bookmarks {

namespace {

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// Helper to get a mutable permanent bookmark node.
BookmarkPermanentNode* AsMutable(const BookmarkPermanentNode* node) {
  return const_cast<BookmarkPermanentNode*>(node);
}

// Comparator used when sorting permanent nodes. Nodes that are initially
// visible are sorted before nodes that are initially hidden.
class VisibilityComparator {
 public:
  explicit VisibilityComparator(BookmarkClient* client) : client_(client) {}

  // Returns true if |n1| precedes |n2|.
  bool operator()(const std::unique_ptr<BookmarkPermanentNode>& n1,
                  const std::unique_ptr<BookmarkPermanentNode>& n2) {
    bool n1_visible = client_->IsPermanentNodeVisible(n1.get());
    bool n2_visible = client_->IsPermanentNodeVisible(n2.get());
    return n1_visible != n2_visible && n1_visible;
  }

 private:
  BookmarkClient* client_;
};

// Comparator used when sorting bookmarks. Folders are sorted first, then
// bookmarks.
class SortComparator {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if |n1| precedes |n2|.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    if (n1->type() == n2->type()) {
      // Types are the same, compare the names.
      if (!collator_)
        return n1->GetTitle() < n2->GetTitle();
      return base::i18n::CompareString16WithCollator(
                 *collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  icu::Collator* collator_;
};

// Delegate that does nothing.
class EmptyUndoDelegate : public BookmarkUndoDelegate {
 public:
  EmptyUndoDelegate() {}
  ~EmptyUndoDelegate() override {}

 private:
  // BookmarkUndoDelegate:
  void SetUndoProvider(BookmarkUndoProvider* provider) override {}
  void OnBookmarkNodeRemoved(BookmarkModel* model,
                             const BookmarkNode* parent,
                             int index,
                             std::unique_ptr<BookmarkNode> node) override {}

  DISALLOW_COPY_AND_ASSIGN(EmptyUndoDelegate);
};

}  // namespace

// BookmarkModel --------------------------------------------------------------

BookmarkModel::BookmarkModel(std::unique_ptr<BookmarkClient> client)
    : client_(std::move(client)),
      loaded_(false),
      root_(GURL()),
      bookmark_bar_node_(nullptr),
      other_node_(nullptr),
      mobile_node_(nullptr),
      next_node_id_(1),
      observers_(base::ObserverListPolicy::EXISTING_ONLY),
      loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      extensive_changes_(0),
      undo_delegate_(nullptr),
      empty_undo_delegate_(new EmptyUndoDelegate) {
  DCHECK(client_);
  client_->Init(this);
}

BookmarkModel::~BookmarkModel() {
  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkModelBeingDeleted(this);

  if (store_.get()) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->BookmarkModelDeleted();
  }
}

void BookmarkModel::Shutdown() {
  if (loaded_)
    return;

  // See comment in HistoryService::ShutdownOnUIThread where this is invoked for
  // details. It is also called when the BookmarkModel is deleted.
  loaded_signal_.Signal();
}

void BookmarkModel::Load(
    PrefService* pref_service,
    const base::FilePath& profile_path,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner) {
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  expanded_state_tracker_.reset(
      new BookmarkExpandedStateTracker(this, pref_service));

  // Load the bookmarks. BookmarkStorage notifies us when done.
  store_.reset(new BookmarkStorage(this, profile_path, io_task_runner.get()));
  store_->LoadBookmarks(CreateLoadDetails(), ui_task_runner);
}

void BookmarkModel::AddObserver(BookmarkModelObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkModel::RemoveObserver(BookmarkModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkModel::BeginExtensiveChanges() {
  if (++extensive_changes_ == 1) {
    for (BookmarkModelObserver& observer : observers_)
      observer.ExtensiveBookmarkChangesBeginning(this);
  }
}

void BookmarkModel::EndExtensiveChanges() {
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (BookmarkModelObserver& observer : observers_)
      observer.ExtensiveBookmarkChangesEnded(this);
  }
}

void BookmarkModel::BeginGroupedChanges() {
  for (BookmarkModelObserver& observer : observers_)
    observer.GroupedBookmarkChangesBeginning(this);
}

void BookmarkModel::EndGroupedChanges() {
  for (BookmarkModelObserver& observer : observers_)
    observer.GroupedBookmarkChangesEnded(this);
}

void BookmarkModel::Remove(const BookmarkNode* node) {
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));
  RemoveAndDeleteNode(AsMutable(node));
}

void BookmarkModel::RemoveAllUserBookmarks() {
  std::set<GURL> removed_urls;
  struct RemoveNodeData {
    const BookmarkNode* parent;
    int index;
    std::unique_ptr<BookmarkNode> node;
  };
  std::vector<RemoveNodeData> removed_node_data_list;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillRemoveAllUserBookmarks(this);

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent bookmark nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    base::AutoLock url_lock(url_lock_);
    for (int i = 0; i < root_.child_count(); ++i) {
      const BookmarkNode* permanent_node = root_.GetChild(i);

      if (!client_->CanBeEditedByUser(permanent_node))
        continue;

      for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
        std::unique_ptr<BookmarkNode> node = RemoveNodeAndGetRemovedUrls(
            AsMutable(permanent_node->GetChild(j)), &removed_urls);
        removed_node_data_list.push_back({permanent_node, j, std::move(node)});
      }
    }
  }
  EndExtensiveChanges();
  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkAllUserNodesRemoved(this, removed_urls);

  BeginGroupedChanges();
  for (auto& removed_node_data : removed_node_data_list) {
    undo_delegate()->OnBookmarkNodeRemoved(this, removed_node_data.parent,
                                           removed_node_data.index,
                                           std::move(removed_node_data.node));
  }
  EndGroupedChanges();
}

void BookmarkModel::Move(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         int index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index, true) ||
      is_root_node(new_parent) || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return;
  }

  const BookmarkNode* old_parent = node->parent();
  int old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  SetDateFolderModified(new_parent, Time::Now());

  if (old_parent == new_parent && index > old_index)
    index--;

  BookmarkNode* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<BookmarkNode> owned_node =
      mutable_old_parent->Remove(AsMutable(node));
  BookmarkNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeMoved(this, old_parent, old_index, new_parent, index);
}

void BookmarkModel::Copy(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         int index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index, true) ||
      is_root_node(new_parent) || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return;
  }

  SetDateFolderModified(new_parent, Time::Now());
  BookmarkNodeData drag_data(node);
  // CloneBookmarkNode will use BookmarkModel methods to do the job, so we
  // don't need to send notifications here.
  CloneBookmarkNode(this, drag_data.elements, new_parent, index, true);

  if (store_.get())
    store_->ScheduleSave();
}

const gfx::Image& BookmarkModel::GetFavicon(const BookmarkNode* node) {
  DCHECK(node);
  if (node->favicon_state() == BookmarkNode::INVALID_FAVICON) {
    BookmarkNode* mutable_node = AsMutable(node);
    LoadFavicon(mutable_node, client_->PreferTouchIcon()
                                  ? favicon_base::IconType::kTouchIcon
                                  : favicon_base::IconType::kFavicon);
  }
  return node->favicon();
}

favicon_base::IconType BookmarkModel::GetFaviconType(const BookmarkNode* node) {
  DCHECK(node);
  return node->favicon_type();
}

void BookmarkModel::SetTitle(const BookmarkNode* node,
                             const base::string16& title) {
  DCHECK(node);

  if (node->GetTitle() == title)
    return;

  if (is_permanent_node(node) && !client_->CanSetPermanentNodeTitle(node)) {
    NOTREACHED();
    return;
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkNode(this, node);

  // The title index doesn't support changing the title, instead we remove then
  // add it back. Only do this for URL nodes. A directory node can have its
  // title changed but should be excluded from the index.
  if (node->is_url())
    index_->Remove(node);
  AsMutable(node)->SetTitle(title);
  if (node->is_url())
    index_->Add(node);

  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChanged(this, node);
}

void BookmarkModel::SetURL(const BookmarkNode* node, const GURL& url) {
  DCHECK(node && !node->is_folder());

  if (node->url() == url)
    return;

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->InvalidateFavicon();
  CancelPendingFaviconLoadRequests(mutable_node);

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkNode(this, node);

  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeFromInternalMaps(mutable_node);
    mutable_node->set_url(url);
    AddNodeToInternalMaps(mutable_node);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChanged(this, node);
}

void BookmarkModel::SetNodeMetaInfo(const BookmarkNode* node,
                                    const std::string& key,
                                    const std::string& value) {
  std::string old_value;
  if (node->GetMetaInfo(key, &old_value) && old_value == value)
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkMetaInfo(this, node);

  if (AsMutable(node)->SetMetaInfo(key, value) && store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkMetaInfoChanged(this, node);
}

void BookmarkModel::SetNodeMetaInfoMap(
    const BookmarkNode* node,
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  const BookmarkNode::MetaInfoMap* old_meta_info_map = node->GetMetaInfoMap();
  if ((!old_meta_info_map && meta_info_map.empty()) ||
      (old_meta_info_map && meta_info_map == *old_meta_info_map))
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkMetaInfo(this, node);

  AsMutable(node)->SetMetaInfoMap(meta_info_map);
  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkMetaInfoChanged(this, node);
}

void BookmarkModel::DeleteNodeMetaInfo(const BookmarkNode* node,
                                       const std::string& key) {
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (!meta_info_map || meta_info_map->find(key) == meta_info_map->end())
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkMetaInfo(this, node);

  if (AsMutable(node)->DeleteMetaInfo(key) && store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkMetaInfoChanged(this, node);
}

void BookmarkModel::AddNonClonedKey(const std::string& key) {
  non_cloned_keys_.insert(key);
}

void BookmarkModel::SetNodeSyncTransactionVersion(
    const BookmarkNode* node,
    int64_t sync_transaction_version) {
  DCHECK(client_->CanSyncNode(node));

  if (sync_transaction_version == node->sync_transaction_version())
    return;

  AsMutable(node)->set_sync_transaction_version(sync_transaction_version);
  if (store_.get())
    store_->ScheduleSave();
}

void BookmarkModel::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                      const GURL& icon_url) {
  std::set<const BookmarkNode*> to_update;
  for (const GURL& page_url : page_urls) {
    std::vector<const BookmarkNode*> nodes;
    GetNodesByURL(page_url, &nodes);
    to_update.insert(nodes.begin(), nodes.end());
  }

  if (!icon_url.is_empty()) {
    // Log Histogram to determine how often |icon_url| is non empty in
    // practice.
    // TODO(pkotwicz): Do something more efficient if |icon_url| is non-empty
    // many times a day for each user.
    UMA_HISTOGRAM_BOOLEAN("Bookmarks.OnFaviconsChangedIconURL", true);

    base::AutoLock url_lock(url_lock_);
    for (const BookmarkNode* node : nodes_ordered_by_url_set_) {
      if (node->icon_url() && icon_url == *node->icon_url())
        to_update.insert(node);
    }
  }

  for (const BookmarkNode* node : to_update) {
    // Rerequest the favicon.
    BookmarkNode* mutable_node = AsMutable(node);
    mutable_node->InvalidateFavicon();
    CancelPendingFaviconLoadRequests(mutable_node);
    for (BookmarkModelObserver& observer : observers_)
      observer.BookmarkNodeFaviconChanged(this, node);
  }
}

void BookmarkModel::SetDateAdded(const BookmarkNode* node, Time date_added) {
  DCHECK(node && !is_permanent_node(node));

  if (node->date_added() == date_added)
    return;

  AsMutable(node)->set_date_added(date_added);

  // Syncing might result in dates newer than the folder's last modified date.
  if (date_added > node->parent()->date_folder_modified()) {
    // Will trigger store_->ScheduleSave().
    SetDateFolderModified(node->parent(), date_added);
  } else if (store_.get()) {
    store_->ScheduleSave();
  }
}

void BookmarkModel::GetNodesByURL(const GURL& url,
                                  std::vector<const BookmarkNode*>* nodes) {
  base::AutoLock url_lock(url_lock_);
  BookmarkNode tmp_node(url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->url() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

const BookmarkNode* BookmarkModel::GetMostRecentlyAddedUserNodeForURL(
    const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetNodesByURL(url, &nodes);
  std::sort(nodes.begin(), nodes.end(), &MoreRecentlyAdded);

  // Look for the first node that the user can edit.
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (client_->CanBeEditedByUser(nodes[i]))
      return nodes[i];
  }

  return nullptr;
}

bool BookmarkModel::HasBookmarks() {
  base::AutoLock url_lock(url_lock_);
  return !nodes_ordered_by_url_set_.empty();
}

bool BookmarkModel::HasNoUserCreatedBookmarksOrFolders() {
  return bookmark_bar_node_->empty() && other_node_->empty() &&
         mobile_node_->empty();
}

bool BookmarkModel::IsBookmarked(const GURL& url) {
  base::AutoLock url_lock(url_lock_);
  return IsBookmarkedNoLock(url);
}

void BookmarkModel::GetBookmarks(
    std::vector<BookmarkModel::URLAndTitle>* bookmarks) {
  base::AutoLock url_lock(url_lock_);
  const GURL* last_url = nullptr;
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    const GURL* url = &((*i)->url());
    // Only add unique URLs.
    if (!last_url || *url != *last_url) {
      BookmarkModel::URLAndTitle bookmark;
      bookmark.url = *url;
      bookmark.title = (*i)->GetTitle();
      bookmarks->push_back(bookmark);
    }
    last_url = url;
  }
}

void BookmarkModel::BlockTillLoaded() {
  loaded_signal_.Wait();
}

const BookmarkNode* BookmarkModel::AddFolder(const BookmarkNode* parent,
                                             int index,
                                             const base::string16& title) {
  return AddFolderWithMetaInfo(parent, index, title, nullptr);
}
const BookmarkNode* BookmarkModel::AddFolderWithMetaInfo(
    const BookmarkNode* parent,
    int index,
    const base::string16& title,
    const BookmarkNode::MetaInfoMap* meta_info) {
  if (!loaded_ || is_root_node(parent) || !IsValidIndex(parent, index, true)) {
    // Can't add to the root.
    NOTREACHED();
    return nullptr;
  }

  std::unique_ptr<BookmarkNode> new_node =
      base::MakeUnique<BookmarkNode>(generate_next_node_id(), GURL());
  new_node->set_date_folder_modified(Time::Now());
  // Folders shouldn't have line breaks in their titles.
  new_node->SetTitle(title);
  new_node->set_type(BookmarkNode::FOLDER);
  if (meta_info)
    new_node->SetMetaInfoMap(*meta_info);

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const BookmarkNode* BookmarkModel::AddURL(const BookmarkNode* parent,
                                          int index,
                                          const base::string16& title,
                                          const GURL& url) {
  return AddURLWithCreationTimeAndMetaInfo(parent, index, title, url,
                                           Time::Now(), nullptr);
}

const BookmarkNode* BookmarkModel::AddURLWithCreationTimeAndMetaInfo(
    const BookmarkNode* parent,
    int index,
    const base::string16& title,
    const GURL& url,
    const Time& creation_time,
    const BookmarkNode::MetaInfoMap* meta_info) {
  if (!loaded_ || !url.is_valid() || is_root_node(parent) ||
      !IsValidIndex(parent, index, true)) {
    NOTREACHED();
    return nullptr;
  }

  // Syncing may result in dates newer than the last modified date.
  if (creation_time > parent->date_folder_modified())
    SetDateFolderModified(parent, creation_time);

  std::unique_ptr<BookmarkNode> new_node =
      base::MakeUnique<BookmarkNode>(generate_next_node_id(), url);
  new_node->SetTitle(title);
  new_node->set_date_added(creation_time);
  new_node->set_type(BookmarkNode::URL);
  if (meta_info)
    new_node->SetMetaInfoMap(*meta_info);

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

void BookmarkModel::SortChildren(const BookmarkNode* parent) {
  DCHECK(client_->CanBeEditedByUser(parent));

  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->child_count() <= 1) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillReorderBookmarkNode(this, parent);

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(nullptr);
  BookmarkNode* mutable_parent = AsMutable(parent);
  std::sort(mutable_parent->children().begin(),
            mutable_parent->children().end(),
            SortComparator(collator.get()));

  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChildrenReordered(this, parent);
}

void BookmarkModel::ReorderChildren(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& ordered_nodes) {
  DCHECK(client_->CanBeEditedByUser(parent));

  // Ensure that all children in |parent| are in |ordered_nodes|.
  DCHECK_EQ(static_cast<size_t>(parent->child_count()), ordered_nodes.size());
  for (const BookmarkNode* node : ordered_nodes)
    DCHECK_EQ(parent, node->parent());

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillReorderBookmarkNode(this, parent);

  if (ordered_nodes.size() > 1) {
    std::map<const BookmarkNode*, int> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i)
      order[ordered_nodes[i]] = i;

    std::vector<std::unique_ptr<BookmarkNode>> new_children(
        ordered_nodes.size());
    BookmarkNode* mutable_parent = AsMutable(parent);
    for (auto& child : mutable_parent->children()) {
      size_t new_location = order[child.get()];
      new_children[new_location] = std::move(child);
    }
    mutable_parent->children().swap(new_children);

    if (store_.get())
      store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChildrenReordered(this, parent);
}

void BookmarkModel::SetDateFolderModified(const BookmarkNode* parent,
                                          const Time time) {
  DCHECK(parent);
  AsMutable(parent)->set_date_folder_modified(time);

  if (store_.get())
    store_->ScheduleSave();
}

void BookmarkModel::ResetDateFolderModified(const BookmarkNode* node) {
  SetDateFolderModified(node, Time());
}

void BookmarkModel::GetBookmarksMatching(const base::string16& text,
                                         size_t max_count,
                                         std::vector<TitledUrlMatch>* matches) {
  GetBookmarksMatching(text, max_count,
                       query_parser::MatchingAlgorithm::DEFAULT, matches);
}

void BookmarkModel::GetBookmarksMatching(
    const base::string16& text,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm,
    std::vector<TitledUrlMatch>* matches) {
  if (!loaded_)
    return;

  index_->GetResultsMatching(text, max_count, matching_algorithm, matches);
}

void BookmarkModel::ClearStore() {
  store_.reset();
}

void BookmarkModel::SetPermanentNodeVisible(BookmarkNode::Type type,
                                            bool value) {
  BookmarkPermanentNode* node = AsMutable(PermanentNode(type));
  node->set_visible(value || client_->IsPermanentNodeVisible(node));
}

const BookmarkPermanentNode* BookmarkModel::PermanentNode(
    BookmarkNode::Type type) {
  DCHECK(loaded_);
  switch (type) {
    case BookmarkNode::BOOKMARK_BAR:
      return bookmark_bar_node_;
    case BookmarkNode::OTHER_NODE:
      return other_node_;
    case BookmarkNode::MOBILE:
      return mobile_node_;
    default:
      NOTREACHED();
      return nullptr;
  }
}

void BookmarkModel::RestoreRemovedNode(const BookmarkNode* parent,
                                       int index,
                                       std::unique_ptr<BookmarkNode> node) {
  BookmarkNode* node_ptr = node.get();
  AddNode(AsMutable(parent), index, std::move(node));

  // We might be restoring a folder node that have already contained a set of
  // child nodes. We need to notify all of them.
  NotifyNodeAddedForAllDescendents(node_ptr);
}

void BookmarkModel::NotifyNodeAddedForAllDescendents(const BookmarkNode* node) {
  for (int i = 0; i < node->child_count(); ++i) {
    for (BookmarkModelObserver& observer : observers_)
      observer.BookmarkNodeAdded(this, node, i);
    NotifyNodeAddedForAllDescendents(node->GetChild(i));
  }
}

bool BookmarkModel::IsBookmarkedNoLock(const GURL& url) {
  BookmarkNode tmp_node(url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void BookmarkModel::RemoveNode(BookmarkNode* node,
                               std::set<GURL>* removed_urls) {
  if (!loaded_ || !node || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  url_lock_.AssertAcquired();
  if (node->is_url()) {
    RemoveNodeFromInternalMaps(node);
    removed_urls->insert(node->url());
  }

  CancelPendingFaviconLoadRequests(node);

  // Recurse through children.
  for (int i = node->child_count() - 1; i >= 0; --i)
    RemoveNode(node->GetChild(i), removed_urls);
}

void BookmarkModel::DoneLoading(std::unique_ptr<BookmarkLoadDetails> details) {
  DCHECK(details);
  if (loaded_) {
    // We should only ever be loaded once.
    NOTREACHED();
    return;
  }

  next_node_id_ = details->max_id();
  if (details->computed_checksum() != details->stored_checksum() ||
      details->ids_reassigned()) {
    // If bookmarks file changed externally, the IDs may have changed
    // externally. In that case, the decoder may have reassigned IDs to make
    // them unique. So when the file has changed externally, we should save the
    // bookmarks file to persist new IDs.
    if (store_.get())
      store_->ScheduleSave();
  }
  std::unique_ptr<BookmarkPermanentNode> owned_bb_node =
      details->owned_bb_node();
  std::unique_ptr<BookmarkPermanentNode> owned_other_folder_node =
      details->owned_other_folder_node();
  std::unique_ptr<BookmarkPermanentNode> owned_mobile_folder_node =
      details->owned_mobile_folder_node();
  index_ = details->owned_index();

  bookmark_bar_node_ = owned_bb_node.get();
  other_node_ = owned_other_folder_node.get();
  mobile_node_ = owned_mobile_folder_node.get();

  // Get any extra nodes and take ownership of them at the |root_|.
  std::vector<std::unique_ptr<BookmarkPermanentNode>> extra_nodes =
      details->owned_extra_nodes();

  // WARNING: order is important here, various places assume the order is
  // constant (but can vary between embedders with the initial visibility
  // of permanent nodes).
  std::vector<std::unique_ptr<BookmarkPermanentNode>> root_children;
  root_children.push_back(std::move(owned_bb_node));
  root_children.push_back(std::move(owned_other_folder_node));
  root_children.push_back(std::move(owned_mobile_folder_node));
  std::move(extra_nodes.begin(), extra_nodes.end(),
            std::back_inserter(root_children));

  std::stable_sort(root_children.begin(),
                   root_children.end(),
                   VisibilityComparator(client_.get()));
  for (size_t i = 0; i < root_children.size(); ++i)
    root_.Add(std::move(root_children[i]), static_cast<int>(i));

  root_.SetMetaInfoMap(details->model_meta_info_map());
  root_.set_sync_transaction_version(details->model_sync_transaction_version());

  {
    base::AutoLock url_lock(url_lock_);
    // Update nodes_ordered_by_url_set_ from the nodes.
    PopulateNodesByURL(&root_);
  }

  loaded_ = true;

  loaded_signal_.Signal();

  // Notify our direct observers.
  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkModelLoaded(this, details->ids_reassigned());
}

void BookmarkModel::RemoveAndDeleteNode(BookmarkNode* node_ptr) {
  std::unique_ptr<BookmarkNode> node;

  const BookmarkNode* parent = node_ptr->parent();
  DCHECK(parent);
  int index = parent->GetIndexOf(node_ptr);
  DCHECK_NE(-1, index);

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillRemoveBookmarks(this, parent, index, node_ptr);

  std::set<GURL> removed_urls;
  {
    base::AutoLock url_lock(url_lock_);
    node = RemoveNodeAndGetRemovedUrls(node_ptr, &removed_urls);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeRemoved(this, parent, index, node.get(), removed_urls);

  undo_delegate()->OnBookmarkNodeRemoved(this, parent, index, std::move(node));
}

void BookmarkModel::RemoveNodeFromInternalMaps(BookmarkNode* node) {
  index_->Remove(node);
  // NOTE: this is called in such a way that url_lock_ is already held. As
  // such, this doesn't explicitly grab the lock.
  url_lock_.AssertAcquired();
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(node);
  DCHECK(i != nodes_ordered_by_url_set_.end());
  // i points to the first node with the URL, advance until we find the
  // node we're removing.
  while (*i != node)
    ++i;
  nodes_ordered_by_url_set_.erase(i);
}

std::unique_ptr<BookmarkNode> BookmarkModel::RemoveNodeAndGetRemovedUrls(
    BookmarkNode* node_ptr,
    std::set<GURL>* removed_urls) {
  // NOTE: this method should be always called with |url_lock_| held.
  // This method does not explicitly acquires a lock.
  url_lock_.AssertAcquired();
  DCHECK(removed_urls);
  BookmarkNode* parent = node_ptr->parent();
  DCHECK(parent);
  std::unique_ptr<BookmarkNode> node = parent->Remove(node_ptr);
  RemoveNode(node_ptr, removed_urls);
  // RemoveNode adds an entry to removed_urls for each node of type URL. As we
  // allow duplicates we need to remove any entries that are still bookmarked.
  for (std::set<GURL>::iterator i = removed_urls->begin();
       i != removed_urls->end();) {
    if (IsBookmarkedNoLock(*i)) {
      // When we erase the iterator pointing at the erasee is
      // invalidated, so using i++ here within the "erase" call is
      // important as it advances the iterator before passing the
      // old value through to erase.
      removed_urls->erase(i++);
    } else {
      ++i;
    }
  }

  return node;
}

BookmarkNode* BookmarkModel::AddNode(BookmarkNode* parent,
                                     int index,
                                     std::unique_ptr<BookmarkNode> node) {
  BookmarkNode* node_ptr = node.get();
  parent->Add(std::move(node), index);

  if (store_.get())
    store_->ScheduleSave();

  {
    base::AutoLock url_lock(url_lock_);
    AddNodeToInternalMaps(node_ptr);
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeAdded(this, parent, index);

  return node_ptr;
}

void BookmarkModel::AddNodeToInternalMaps(BookmarkNode* node) {
  url_lock_.AssertAcquired();
  if (node->is_url()) {
    index_->Add(node);
    nodes_ordered_by_url_set_.insert(node);
  }
  for (int i = 0; i < node->child_count(); ++i)
    AddNodeToInternalMaps(node->GetChild(i));
}

bool BookmarkModel::IsValidIndex(const BookmarkNode* parent,
                                 int index,
                                 bool allow_end) {
  return (parent && parent->is_folder() &&
          (index >= 0 && (index < parent->child_count() ||
                          (allow_end && index == parent->child_count()))));
}

BookmarkPermanentNode* BookmarkModel::CreatePermanentNode(
    BookmarkNode::Type type) {
  DCHECK(type == BookmarkNode::BOOKMARK_BAR ||
         type == BookmarkNode::OTHER_NODE ||
         type == BookmarkNode::MOBILE);
  BookmarkPermanentNode* node =
      new BookmarkPermanentNode(generate_next_node_id());
  node->set_type(type);
  node->set_visible(client_->IsPermanentNodeVisible(node));

  int title_id;
  switch (type) {
    case BookmarkNode::BOOKMARK_BAR:
      title_id = IDS_BOOKMARK_BAR_FOLDER_NAME;
      break;
    case BookmarkNode::OTHER_NODE:
      title_id = IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME;
      break;
    case BookmarkNode::MOBILE:
      title_id = IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME;
      break;
    default:
      NOTREACHED();
      title_id = IDS_BOOKMARK_BAR_FOLDER_NAME;
      break;
  }
  node->SetTitle(l10n_util::GetStringUTF16(title_id));
  return node;
}

void BookmarkModel::OnFaviconDataAvailable(
    BookmarkNode* node,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconImageResult& image_result) {
  DCHECK(node);
  node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  node->set_favicon_state(BookmarkNode::LOADED_FAVICON);
  if (!image_result.image.IsEmpty()) {
    node->set_favicon_type(icon_type);
    node->set_favicon(image_result.image);
    node->set_icon_url(image_result.icon_url);
    FaviconLoaded(node);
  } else if (icon_type == favicon_base::IconType::kTouchIcon) {
    // Couldn't load the touch icon, fallback to the regular favicon.
    DCHECK(client_->PreferTouchIcon());
    LoadFavicon(node, favicon_base::IconType::kFavicon);
  } else {
    // No favicon available, but we still notify observers.
    FaviconLoaded(node);
  }
}

void BookmarkModel::LoadFavicon(BookmarkNode* node,
                                favicon_base::IconType icon_type) {
  if (node->is_folder())
    return;

  DCHECK(node->url().is_valid());
  node->set_favicon_state(BookmarkNode::LOADING_FAVICON);
  base::CancelableTaskTracker::TaskId taskId =
      client_->GetFaviconImageForPageURL(
          node->url(),
          icon_type,
          base::Bind(
              &BookmarkModel::OnFaviconDataAvailable,
              base::Unretained(this),
              node,
              icon_type),
          &cancelable_task_tracker_);
  if (taskId != base::CancelableTaskTracker::kBadTaskId)
    node->set_favicon_load_task_id(taskId);
}

void BookmarkModel::FaviconLoaded(const BookmarkNode* node) {
  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeFaviconChanged(this, node);
}

void BookmarkModel::CancelPendingFaviconLoadRequests(BookmarkNode* node) {
  if (node->favicon_load_task_id() != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(node->favicon_load_task_id());
    node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  }
}

void BookmarkModel::PopulateNodesByURL(BookmarkNode* node) {
  // NOTE: this is called with url_lock_ already held. As such, this doesn't
  // explicitly grab the lock.
  if (node->is_url())
    nodes_ordered_by_url_set_.insert(node);
  for (int i = 0; i < node->child_count(); ++i)
    PopulateNodesByURL(node->GetChild(i));
}

int64_t BookmarkModel::generate_next_node_id() {
  return next_node_id_++;
}

std::unique_ptr<BookmarkLoadDetails> BookmarkModel::CreateLoadDetails() {
  BookmarkPermanentNode* bb_node =
      CreatePermanentNode(BookmarkNode::BOOKMARK_BAR);
  BookmarkPermanentNode* other_node =
      CreatePermanentNode(BookmarkNode::OTHER_NODE);
  BookmarkPermanentNode* mobile_node =
      CreatePermanentNode(BookmarkNode::MOBILE);
  std::unique_ptr<TitledUrlNodeSorter> node_sorter =
      base::MakeUnique<TypedCountSorter>(client_.get());
  return std::unique_ptr<BookmarkLoadDetails>(new BookmarkLoadDetails(
      bb_node, other_node, mobile_node, client_->GetLoadExtraNodesCallback(),
      new TitledUrlIndex(std::move(node_sorter)), next_node_id_));
}

void BookmarkModel::SetUndoDelegate(BookmarkUndoDelegate* undo_delegate) {
  undo_delegate_ = undo_delegate;
  if (undo_delegate_)
    undo_delegate_->SetUndoProvider(this);
}

BookmarkUndoDelegate* BookmarkModel::undo_delegate() const {
  return undo_delegate_ ? undo_delegate_ : empty_undo_delegate_.get();
}

}  // namespace bookmarks
