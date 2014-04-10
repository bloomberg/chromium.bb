// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/string_compare.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"
#include "chrome/browser/bookmarks/bookmark_index.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/bookmarks/bookmark_title_match.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_changed_details.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/favicon/favicon_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_util.h"

using base::Time;

namespace {

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// Whitespace characters to strip from bookmark titles.
const base::char16 kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

}  // namespace

// BookmarkNode ---------------------------------------------------------------

const int64 BookmarkNode::kInvalidSyncTransactionVersion = -1;

BookmarkNode::BookmarkNode(const GURL& url)
    : url_(url) {
  Initialize(0);
}

BookmarkNode::BookmarkNode(int64 id, const GURL& url)
    : url_(url) {
  Initialize(id);
}

BookmarkNode::~BookmarkNode() {
}

void BookmarkNode::SetTitle(const base::string16& title) {
  // Replace newlines and other problematic whitespace characters in
  // folder/bookmark names with spaces.
  base::string16 trimmed_title;
  base::ReplaceChars(title, kInvalidChars, base::ASCIIToUTF16(" "),
                     &trimmed_title);
  ui::TreeNode<BookmarkNode>::SetTitle(trimmed_title);
}

bool BookmarkNode::IsVisible() const {
  return true;
}

bool BookmarkNode::GetMetaInfo(const std::string& key,
                               std::string* value) const {
  if (!meta_info_map_)
    return false;

  MetaInfoMap::const_iterator it = meta_info_map_->find(key);
  if (it == meta_info_map_->end())
    return false;

  *value = it->second;
  return true;
}

bool BookmarkNode::SetMetaInfo(const std::string& key,
                               const std::string& value) {
  if (!meta_info_map_)
    meta_info_map_.reset(new MetaInfoMap);

  MetaInfoMap::iterator it = meta_info_map_->find(key);
  if (it == meta_info_map_->end()) {
    (*meta_info_map_)[key] = value;
    return true;
  }
  // Key already in map, check if the value has changed.
  if (it->second == value)
    return false;
  it->second = value;
  return true;
}

bool BookmarkNode::DeleteMetaInfo(const std::string& key) {
  if (!meta_info_map_)
    return false;
  bool erased = meta_info_map_->erase(key) != 0;
  if (meta_info_map_->empty())
    meta_info_map_.reset();
  return erased;
}

void BookmarkNode::SetMetaInfoMap(const MetaInfoMap& meta_info_map) {
  if (meta_info_map.empty())
    meta_info_map_.reset();
  else
    meta_info_map_.reset(new MetaInfoMap(meta_info_map));
}

const BookmarkNode::MetaInfoMap* BookmarkNode::GetMetaInfoMap() const {
  return meta_info_map_.get();
}

void BookmarkNode::Initialize(int64 id) {
  id_ = id;
  type_ = url_.is_empty() ? FOLDER : URL;
  date_added_ = Time::Now();
  favicon_state_ = INVALID_FAVICON;
  favicon_load_task_id_ = base::CancelableTaskTracker::kBadTaskId;
  meta_info_map_.reset();
  sync_transaction_version_ = kInvalidSyncTransactionVersion;
}

void BookmarkNode::InvalidateFavicon() {
  icon_url_ = GURL();
  favicon_ = gfx::Image();
  favicon_state_ = INVALID_FAVICON;
}

namespace {

// Comparator used when sorting bookmarks. Folders are sorted first, then
// bookmarks.
class SortComparator : public std::binary_function<const BookmarkNode*,
                                                   const BookmarkNode*,
                                                   bool> {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if |n1| preceeds |n2|.
  bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) {
    if (n1->type() == n2->type()) {
      // Types are the same, compare the names.
      if (!collator_)
        return n1->GetTitle() < n2->GetTitle();
      return base::i18n::CompareString16WithCollator(
          collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

// BookmarkPermanentNode -------------------------------------------------------

BookmarkPermanentNode::BookmarkPermanentNode(int64 id)
    : BookmarkNode(id, GURL()),
      visible_(true) {
}

BookmarkPermanentNode::~BookmarkPermanentNode() {
}

bool BookmarkPermanentNode::IsVisible() const {
  return visible_ || !empty();
}

// BookmarkModel --------------------------------------------------------------

BookmarkModel::BookmarkModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
      root_(GURL()),
      bookmark_bar_node_(NULL),
      other_node_(NULL),
      mobile_node_(NULL),
      next_node_id_(1),
      observers_(ObserverList<BookmarkModelObserver>::NOTIFY_EXISTING_ONLY),
      loaded_signal_(true, false),
      extensive_changes_(0) {
  if (!profile_) {
    // Profile is null during testing.
    DoneLoading(CreateLoadDetails());
  }
}

BookmarkModel::~BookmarkModel() {
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkModelBeingDeleted(this));

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
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  expanded_state_tracker_.reset(
      new BookmarkExpandedStateTracker(this, profile_->GetPrefs()));

  // Listen for changes to favicons so that we can update the favicon of the
  // node appropriately.
  registrar_.Add(this, chrome::NOTIFICATION_FAVICON_CHANGED,
                 content::Source<Profile>(profile_));

  // Load the bookmarks. BookmarkStorage notifies us when done.
  store_ = new BookmarkStorage(profile_, this, task_runner.get());
  store_->LoadBookmarks(CreateLoadDetails());
}

const BookmarkNode* BookmarkModel::GetParentForNewNodes() {
  std::vector<const BookmarkNode*> nodes =
      bookmark_utils::GetMostRecentlyModifiedFolders(this, 1);
  DCHECK(!nodes.empty());  // This list is always padded with default folders.
  return nodes[0];
}

void BookmarkModel::AddObserver(BookmarkModelObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkModel::RemoveObserver(BookmarkModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkModel::BeginExtensiveChanges() {
  if (++extensive_changes_ == 1) {
    FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                      ExtensiveBookmarkChangesBeginning(this));
  }
}

void BookmarkModel::EndExtensiveChanges() {
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                      ExtensiveBookmarkChangesEnded(this));
  }
}

void BookmarkModel::BeginGroupedChanges() {
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    GroupedBookmarkChangesBeginning(this));
}

void BookmarkModel::EndGroupedChanges() {
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    GroupedBookmarkChangesEnded(this));
}

void BookmarkModel::Remove(const BookmarkNode* parent, int index) {
  if (!loaded_ || !IsValidIndex(parent, index, false) || is_root_node(parent)) {
    NOTREACHED();
    return;
  }
  RemoveAndDeleteNode(AsMutable(parent->GetChild(index)));
}

void BookmarkModel::RemoveAll() {
  std::set<GURL> removed_urls;
  ScopedVector<BookmarkNode> removed_nodes;

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillRemoveAllBookmarks(this));

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent bookmark nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    base::AutoLock url_lock(url_lock_);
    for (int i = 0; i < root_.child_count(); ++i) {
      BookmarkNode* permanent_node = root_.GetChild(i);
      for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
        BookmarkNode* child_node = permanent_node->GetChild(j);
        removed_nodes.push_back(child_node);
        RemoveNodeAndGetRemovedUrls(child_node, &removed_urls);
      }
    }
  }
  EndExtensiveChanges();
  if (store_.get())
    store_->ScheduleSave();

  NotifyHistoryAboutRemovedBookmarks(removed_urls);

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkAllNodesRemoved(this));
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
  BookmarkNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(AsMutable(node), index);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeMoved(this, old_parent, old_index,
                                      new_parent, index));
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
  std::vector<BookmarkNodeData::Element> elements(drag_data.elements);
  // CloneBookmarkNode will use BookmarkModel methods to do the job, so we
  // don't need to send notifications here.
  bookmark_utils::CloneBookmarkNode(this, elements, new_parent, index, true);

  if (store_.get())
    store_->ScheduleSave();
}

const gfx::Image& BookmarkModel::GetFavicon(const BookmarkNode* node) {
  DCHECK(node);
  if (node->favicon_state() == BookmarkNode::INVALID_FAVICON) {
    BookmarkNode* mutable_node = AsMutable(node);
    mutable_node->set_favicon_state(BookmarkNode::LOADING_FAVICON);
    LoadFavicon(mutable_node);
  }
  return node->favicon();
}

void BookmarkModel::SetTitle(const BookmarkNode* node, const base::string16& title) {
  if (!node) {
    NOTREACHED();
    return;
  }
  if (node->GetTitle() == title)
    return;

  if (is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillChangeBookmarkNode(this, node));

  // The title index doesn't support changing the title, instead we remove then
  // add it back.
  index_->Remove(node);
  AsMutable(node)->SetTitle(title);
  index_->Add(node);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChanged(this, node));
}

void BookmarkModel::SetURL(const BookmarkNode* node, const GURL& url) {
  if (!node) {
    NOTREACHED();
    return;
  }

  // We cannot change the URL of a folder.
  if (node->is_folder()) {
    NOTREACHED();
    return;
  }

  if (node->url() == url)
    return;

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->InvalidateFavicon();
  CancelPendingFaviconLoadRequests(mutable_node);

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillChangeBookmarkNode(this, node));

  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeFromURLSet(mutable_node);
    mutable_node->set_url(url);
    nodes_ordered_by_url_set_.insert(mutable_node);
  }

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChanged(this, node));
}

void BookmarkModel::SetNodeMetaInfo(const BookmarkNode* node,
                                    const std::string& key,
                                    const std::string& value) {
  std::string old_value;
  if (node->GetMetaInfo(key, &old_value) && old_value == value)
    return;

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillChangeBookmarkMetaInfo(this, node));

  if (AsMutable(node)->SetMetaInfo(key, value) && store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkMetaInfoChanged(this, node));
}

void BookmarkModel::SetNodeMetaInfoMap(
    const BookmarkNode* node,
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  const BookmarkNode::MetaInfoMap* old_meta_info_map = node->GetMetaInfoMap();
  if ((!old_meta_info_map && meta_info_map.empty()) ||
      (old_meta_info_map && meta_info_map == *old_meta_info_map))
    return;

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillChangeBookmarkMetaInfo(this, node));

  AsMutable(node)->SetMetaInfoMap(meta_info_map);
  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkMetaInfoChanged(this, node));
}

void BookmarkModel::DeleteNodeMetaInfo(const BookmarkNode* node,
                                       const std::string& key) {
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (!meta_info_map || meta_info_map->find(key) == meta_info_map->end())
    return;

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillChangeBookmarkMetaInfo(this, node));

  if (AsMutable(node)->DeleteMetaInfo(key) && store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkMetaInfoChanged(this, node));
}

void BookmarkModel::SetNodeSyncTransactionVersion(
    const BookmarkNode* node,
    int64 sync_transaction_version) {
  if (sync_transaction_version == node->sync_transaction_version())
    return;

  AsMutable(node)->set_sync_transaction_version(sync_transaction_version);
  if (store_.get())
    store_->ScheduleSave();
}

void BookmarkModel::SetDateAdded(const BookmarkNode* node,
                                 base::Time date_added) {
  if (!node) {
    NOTREACHED();
    return;
  }

  if (node->date_added() == date_added)
    return;

  if (is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

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

const BookmarkNode* BookmarkModel::GetMostRecentlyAddedNodeForURL(
    const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  GetNodesByURL(url, &nodes);
  if (nodes.empty())
    return NULL;

  std::sort(nodes.begin(), nodes.end(), &bookmark_utils::MoreRecentlyAdded);
  return nodes.front();
}

bool BookmarkModel::HasBookmarks() {
  base::AutoLock url_lock(url_lock_);
  return !nodes_ordered_by_url_set_.empty();
}

bool BookmarkModel::IsBookmarked(const GURL& url) {
  base::AutoLock url_lock(url_lock_);
  return IsBookmarkedNoLock(url);
}

void BookmarkModel::GetBookmarks(
    std::vector<BookmarkService::URLAndTitle>* bookmarks) {
  base::AutoLock url_lock(url_lock_);
  const GURL* last_url = NULL;
  for (NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    const GURL* url = &((*i)->url());
    // Only add unique URLs.
    if (!last_url || *url != *last_url) {
      BookmarkService::URLAndTitle bookmark;
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

const BookmarkNode* BookmarkModel::GetNodeByID(int64 id) const {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(&root_, id);
}

const BookmarkNode* BookmarkModel::AddFolder(const BookmarkNode* parent,
                                             int index,
                                             const base::string16& title) {
  if (!loaded_ || is_root_node(parent) || !IsValidIndex(parent, index, true)) {
    // Can't add to the root.
    NOTREACHED();
    return NULL;
  }

  BookmarkNode* new_node = new BookmarkNode(generate_next_node_id(), GURL());
  new_node->set_date_folder_modified(Time::Now());
  // Folders shouldn't have line breaks in their titles.
  new_node->SetTitle(title);
  new_node->set_type(BookmarkNode::FOLDER);

  return AddNode(AsMutable(parent), index, new_node);
}

const BookmarkNode* BookmarkModel::AddURL(const BookmarkNode* parent,
                                          int index,
                                          const base::string16& title,
                                          const GURL& url) {
  return AddURLWithCreationTime(parent, index,
                                base::CollapseWhitespace(title, false),
                                url, Time::Now());
}

const BookmarkNode* BookmarkModel::AddURLWithCreationTime(
    const BookmarkNode* parent,
    int index,
    const base::string16& title,
    const GURL& url,
    const Time& creation_time) {
  if (!loaded_ || !url.is_valid() || is_root_node(parent) ||
      !IsValidIndex(parent, index, true)) {
    NOTREACHED();
    return NULL;
  }

  // Syncing may result in dates newer than the last modified date.
  if (creation_time > parent->date_folder_modified())
    SetDateFolderModified(parent, creation_time);

  BookmarkNode* new_node = new BookmarkNode(generate_next_node_id(), url);
  new_node->SetTitle(title);
  new_node->set_date_added(creation_time);
  new_node->set_type(BookmarkNode::URL);

  {
    // Only hold the lock for the duration of the insert.
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node);
  }

  return AddNode(AsMutable(parent), index, new_node);
}

void BookmarkModel::SortChildren(const BookmarkNode* parent) {
  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->child_count() <= 1) {
    return;
  }

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillReorderBookmarkNode(this, parent));

  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  BookmarkNode* mutable_parent = AsMutable(parent);
  std::sort(mutable_parent->children().begin(),
            mutable_parent->children().end(),
            SortComparator(collator.get()));

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChildrenReordered(this, parent));
}

void BookmarkModel::ReorderChildren(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& ordered_nodes) {
  // Ensure that all children in |parent| are in |ordered_nodes|.
  DCHECK_EQ(static_cast<size_t>(parent->child_count()), ordered_nodes.size());
  for (size_t i = 0; i < ordered_nodes.size(); ++i)
    DCHECK_EQ(parent, ordered_nodes[i]->parent());

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillReorderBookmarkNode(this, parent));

  AsMutable(parent)->SetChildren(
      *(reinterpret_cast<const std::vector<BookmarkNode*>*>(&ordered_nodes)));

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeChildrenReordered(this, parent));
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

void BookmarkModel::GetBookmarksWithTitlesMatching(
    const base::string16& text,
    size_t max_count,
    std::vector<BookmarkTitleMatch>* matches) {
  if (!loaded_)
    return;

  index_->GetBookmarksWithTitlesMatching(text, max_count, matches);
}

void BookmarkModel::ClearStore() {
  registrar_.RemoveAll();
  store_ = NULL;
}

void BookmarkModel::SetPermanentNodeVisible(BookmarkNode::Type type,
                                            bool value) {
  DCHECK(loaded_);
  switch (type) {
    case BookmarkNode::BOOKMARK_BAR:
      bookmark_bar_node_->set_visible(value);
      break;
    case BookmarkNode::OTHER_NODE:
      other_node_->set_visible(value);
      break;
    case BookmarkNode::MOBILE:
      mobile_node_->set_visible(value);
      break;
    default:
      NOTREACHED();
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
    RemoveNodeFromURLSet(node);
    removed_urls->insert(node->url());
    index_->Remove(node);
  }

  CancelPendingFaviconLoadRequests(node);

  // Recurse through children.
  for (int i = node->child_count() - 1; i >= 0; --i)
    RemoveNode(node->GetChild(i), removed_urls);
}

void BookmarkModel::DoneLoading(BookmarkLoadDetails* details_delete_me) {
  DCHECK(details_delete_me);
  scoped_ptr<BookmarkLoadDetails> details(details_delete_me);
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
  bookmark_bar_node_ = details->release_bb_node();
  other_node_ = details->release_other_folder_node();
  mobile_node_ = details->release_mobile_folder_node();
  index_.reset(details->release_index());

  // WARNING: order is important here, various places assume the order is
  // constant.
  root_.Add(bookmark_bar_node_, 0);
  root_.Add(other_node_, 1);
  root_.Add(mobile_node_, 2);

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
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkModelLoaded(this, details->ids_reassigned()));
}

void BookmarkModel::RemoveAndDeleteNode(BookmarkNode* delete_me) {
  scoped_ptr<BookmarkNode> node(delete_me);

  const BookmarkNode* parent = node->parent();
  DCHECK(parent);
  int index = parent->GetIndexOf(node.get());

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    OnWillRemoveBookmarks(this, parent, index, node.get()));

  std::set<GURL> removed_urls;
  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeAndGetRemovedUrls(node.get(), &removed_urls);
  }

  if (store_.get())
    store_->ScheduleSave();

  NotifyHistoryAboutRemovedBookmarks(removed_urls);

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeRemoved(this, parent, index, node.get()));
}

void BookmarkModel::RemoveNodeFromURLSet(BookmarkNode* node) {
  // NOTE: this is called in such a way that url_lock_ is already held. As
  // such, this doesn't explicitly grab the lock.
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(node);
  DCHECK(i != nodes_ordered_by_url_set_.end());
  // i points to the first node with the URL, advance until we find the
  // node we're removing.
  while (*i != node)
    ++i;
  nodes_ordered_by_url_set_.erase(i);
}

void BookmarkModel::RemoveNodeAndGetRemovedUrls(BookmarkNode* node,
                                                std::set<GURL>* removed_urls) {
  // NOTE: this method should be always called with |url_lock_| held.
  // This method does not explicitly acquires a lock.
  url_lock_.AssertAcquired();
  DCHECK(removed_urls);
  BookmarkNode* parent = AsMutable(node->parent());
  DCHECK(parent);
  parent->Remove(node);
  RemoveNode(node, removed_urls);
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
}

void BookmarkModel::NotifyHistoryAboutRemovedBookmarks(
    const std::set<GURL>& removed_bookmark_urls) const {
  if (removed_bookmark_urls.empty()) {
    // No point in sending out notification if the starred state didn't change.
    return;
  }

  if (profile_) {
    HistoryService* history =
        HistoryServiceFactory::GetForProfile(profile_,
                                             Profile::EXPLICIT_ACCESS);
    if (history)
      history->URLsNoLongerBookmarked(removed_bookmark_urls);
  }
}

BookmarkNode* BookmarkModel::AddNode(BookmarkNode* parent,
                                     int index,
                                     BookmarkNode* node) {
  parent->Add(node, index);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeAdded(this, parent, index));

  index_->Add(node);

  return node;
}

const BookmarkNode* BookmarkModel::GetNodeByID(const BookmarkNode* node,
                                               int64 id) const {
  if (node->id() == id)
    return node;

  for (int i = 0, child_count = node->child_count(); i < child_count; ++i) {
    const BookmarkNode* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
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
  if (type == BookmarkNode::MOBILE)
    node->set_visible(false);  // Mobile node is initially hidden.

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
  node->set_type(type);
  return node;
}

void BookmarkModel::OnFaviconDataAvailable(
    BookmarkNode* node,
    const chrome::FaviconImageResult& image_result) {
  DCHECK(node);
  node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  node->set_favicon_state(BookmarkNode::LOADED_FAVICON);
  if (!image_result.image.IsEmpty()) {
    node->set_favicon(image_result.image);
    node->set_icon_url(image_result.icon_url);
    FaviconLoaded(node);
  }
}

void BookmarkModel::LoadFavicon(BookmarkNode* node) {
  if (node->is_folder())
    return;

  DCHECK(node->url().is_valid());
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;
  base::CancelableTaskTracker::TaskId taskId =
      favicon_service->GetFaviconImageForURL(
          FaviconService::FaviconForURLParams(
              node->url(), chrome::FAVICON, gfx::kFaviconSize),
          base::Bind(&BookmarkModel::OnFaviconDataAvailable,
                     base::Unretained(this),
                     node),
          &cancelable_task_tracker_);
  node->set_favicon_load_task_id(taskId);
}

void BookmarkModel::FaviconLoaded(const BookmarkNode* node) {
  FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                    BookmarkNodeFaviconChanged(this, node));
}

void BookmarkModel::CancelPendingFaviconLoadRequests(BookmarkNode* node) {
  if (node->favicon_load_task_id() != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(node->favicon_load_task_id());
    node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  }
}

void BookmarkModel::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FAVICON_CHANGED: {
      // Prevent the observers from getting confused for multiple favicon loads.
      content::Details<FaviconChangedDetails> favicon_details(details);
      for (std::set<GURL>::const_iterator i = favicon_details->urls.begin();
           i != favicon_details->urls.end(); ++i) {
        std::vector<const BookmarkNode*> nodes;
        GetNodesByURL(*i, &nodes);
        for (size_t i = 0; i < nodes.size(); ++i) {
          // Got an updated favicon, for a URL, do a new request.
          BookmarkNode* node = AsMutable(nodes[i]);
          node->InvalidateFavicon();
          CancelPendingFaviconLoadRequests(node);
          FOR_EACH_OBSERVER(BookmarkModelObserver, observers_,
                            BookmarkNodeFaviconChanged(this, node));
        }
      }
      break;
    }

    default:
      NOTREACHED();
      break;
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

int64 BookmarkModel::generate_next_node_id() {
  return next_node_id_++;
}

BookmarkLoadDetails* BookmarkModel::CreateLoadDetails() {
  BookmarkPermanentNode* bb_node =
      CreatePermanentNode(BookmarkNode::BOOKMARK_BAR);
  BookmarkPermanentNode* other_node =
      CreatePermanentNode(BookmarkNode::OTHER_NODE);
  BookmarkPermanentNode* mobile_node =
      CreatePermanentNode(BookmarkNode::MOBILE);
  return new BookmarkLoadDetails(bb_node, other_node, mobile_node,
                                 new BookmarkIndex(profile_),
                                 next_node_id_);
}
