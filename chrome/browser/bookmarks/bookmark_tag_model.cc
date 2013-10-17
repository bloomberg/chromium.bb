// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_tag_model.h"

#include "base/auto_reset.h"
#include "base/json/json_string_value_serializer.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_tag_model_observer.h"
#include "ui/base/models/tree_node_iterator.h"

namespace {
// The key used to store the tag list in the metainfo of a bookmark.
const char* TAG_KEY = "TAG_KEY";

// Comparator to sort tags by usage.
struct TagComparator {
  TagComparator(std::map<BookmarkTag, unsigned int>& tags) : tags_(tags) {
  }
  ~TagComparator() {}

  bool operator()(const BookmarkTag& a, const BookmarkTag& b) {
    return (tags_[a] < tags_[b]);
  }

  std::map<BookmarkTag, unsigned int>& tags_;
};

// The tags are currently stored in the BookmarkNode's metaInfo in JSON
// format. This function extracts the info from there and returns it in
// digestible format.
// If the Bookmark was never tagged before it is implicitely tagged with the
// title of all its ancestors in the BookmarkModel.
std::set<BookmarkTag> ExtractTagsFromBookmark(const BookmarkNode* bookmark) {
  // This is awful BTW. Metainfo is itself an encoded JSON, and here we decode
  // another layer.

  // Retrieve the encodedData from the bookmark. If there is no encoded data
  // at all returns the name of all the ancestors as separate tags.
  std::string encoded;
  if (!bookmark->GetMetaInfo(TAG_KEY, &encoded)) {
    std::set<BookmarkTag> tags;
    const BookmarkNode* folder = bookmark->parent();
    while (folder && folder->type() == BookmarkNode::FOLDER) {
      BookmarkTag trimmed_tag = CollapseWhitespace(folder->GetTitle(), true);
      if (!trimmed_tag.empty())
        tags.insert(trimmed_tag);
      folder = folder->parent();
    }
    return tags;
  }

  // Decode into a base::Value. If the data is not encoded properly as a list
  // return an empty result.
  JSONStringValueSerializer serializer(&encoded);
  int error_code = 0;
  std::string error_message;
  scoped_ptr<base::Value> result(serializer.Deserialize(&error_code,
                                                        &error_message));

  if (error_code || !result->IsType(base::Value::TYPE_LIST))
    return std::set<BookmarkTag>();

  base::ListValue* list = NULL;
  if (!result->GetAsList(&list) || list->empty())
    return std::set<BookmarkTag>();

  // Build the set.
  std::set<BookmarkTag> return_value;

  for (base::ListValue::iterator it = list->begin();
       it != list->end(); ++it) {
    base::Value* item = *it;
    BookmarkTag tag;
    if (!item->GetAsString(&tag))
      continue;
    return_value.insert(tag);
  }
  return return_value;
}
}  // namespace

BookmarkTagModel::BookmarkTagModel(BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model),
      loaded_(false),
      observers_(ObserverList<BookmarkTagModelObserver>::NOTIFY_EXISTING_ONLY),
      inhibit_change_notifications_(false) {
  bookmark_model_->AddObserver(this);
  if (bookmark_model_->loaded())
    Load();
}

BookmarkTagModel::~BookmarkTagModel() {
  if (bookmark_model_)
    bookmark_model_->RemoveObserver(this);
}

// BookmarkModel forwarding.

void BookmarkTagModel::AddObserver(BookmarkTagModelObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkTagModel::RemoveObserver(BookmarkTagModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkTagModel::BeginExtensiveChanges() {
  DCHECK(bookmark_model_);
  bookmark_model_->BeginExtensiveChanges();
}

void BookmarkTagModel::EndExtensiveChanges() {
  DCHECK(bookmark_model_);
  bookmark_model_->EndExtensiveChanges();
}

bool BookmarkTagModel::IsDoingExtensiveChanges() const {
  DCHECK(bookmark_model_);
  return bookmark_model_->IsDoingExtensiveChanges();
}

void BookmarkTagModel::Remove(const BookmarkNode* bookmark) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  const BookmarkNode* parent = bookmark->parent();
  bookmark_model_->Remove(parent, parent->GetIndexOf(bookmark));
}

void BookmarkTagModel::RemoveAll() {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  bookmark_model_->RemoveAll();
}

const gfx::Image& BookmarkTagModel::GetFavicon(const BookmarkNode* bookmark) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  return bookmark_model_->GetFavicon(bookmark);
}

void BookmarkTagModel::SetTitle(const BookmarkNode* bookmark,
                                const string16& title) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  bookmark_model_->SetTitle(bookmark, title);
}

void BookmarkTagModel::SetURL(const BookmarkNode* bookmark, const GURL& url) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  bookmark_model_->SetURL(bookmark, url);
}

void BookmarkTagModel::SetDateAdded(const BookmarkNode* bookmark,
                                    base::Time date_added) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  bookmark_model_->SetDateAdded(bookmark, date_added);
}

const BookmarkNode*
    BookmarkTagModel::GetMostRecentlyAddedBookmarkForURL(const GURL& url) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);
  return bookmark_model_->GetMostRecentlyAddedNodeForURL(url);
}

// Tags specific code.

const BookmarkNode* BookmarkTagModel::AddURL(
    const string16& title,
    const GURL& url,
    const std::set<BookmarkTag>& tags) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);

  const BookmarkNode* bookmark;
  {
    base::AutoReset<bool> inhibitor(&inhibit_change_notifications_, true);
    const BookmarkNode* parent = bookmark_model_->GetParentForNewNodes();
    bookmark = bookmark_model_->AddURL(parent, 0, title, url);
    AddTagsToBookmark(tags, bookmark);
  }
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    BookmarkNodeAdded(this, bookmark));

  return bookmark;
}

std::set<BookmarkTag> BookmarkTagModel::GetTagsForBookmark(
    const BookmarkNode* bookmark) {
  DCHECK(loaded_);
  return bookmark_to_tags_[bookmark];
}

void BookmarkTagModel::AddTagsToBookmark(
    const std::set<BookmarkTag>& tags,
    const BookmarkNode* bookmark) {
  DCHECK(bookmark_model_);
  std::set<BookmarkTag> all_tags(GetTagsForBookmark(bookmark));
  for (std::set<BookmarkTag>::const_iterator it = tags.begin();
       it != tags.end(); ++it) {
    BookmarkTag trimmed_tag = CollapseWhitespace(*it, true);
    if (trimmed_tag.empty())
      continue;
    all_tags.insert(trimmed_tag);
  }
  SetTagsOnBookmark(all_tags, bookmark);
}

void BookmarkTagModel::AddTagsToBookmarks(
    const std::set<BookmarkTag>& tags,
    const std::set<const BookmarkNode*>& bookmarks) {
  for (std::set<const BookmarkNode*>::const_iterator it = bookmarks.begin();
      it != bookmarks.end(); ++it) {
    AddTagsToBookmark(tags, *it);
  }
}

void BookmarkTagModel::RemoveTagsFromBookmark(
    const std::set<BookmarkTag>& tags,
    const BookmarkNode* bookmark) {
  std::set<BookmarkTag> all_tags(GetTagsForBookmark(bookmark));
  for (std::set<BookmarkTag>::const_iterator it = tags.begin();
       it != tags.end(); ++it) {
    all_tags.erase(*it);
  }
  SetTagsOnBookmark(all_tags, bookmark);
}

void BookmarkTagModel::RemoveTagsFromBookmarks(
    const std::set<BookmarkTag>& tags,
    const std::set<const BookmarkNode*>& bookmarks){
  for (std::set<const BookmarkNode*>::const_iterator it = bookmarks.begin();
      it != bookmarks.end(); ++it) {
    RemoveTagsFromBookmark(tags, *it);
  }
}

std::set<const BookmarkNode*> BookmarkTagModel::BookmarksForTags(
    const std::set<BookmarkTag>& tags) {
  DCHECK(loaded_);
  // Count for each tags how many times a bookmark appeared.
  std::map<const BookmarkNode*, size_t> bookmark_counts;
  for (std::set<BookmarkTag>::const_iterator it = tags.begin();
       it != tags.end(); ++it) {
    const std::set<const BookmarkNode*>& subset(tag_to_bookmarks_[*it]);
    for (std::set<const BookmarkNode*>::const_iterator tag_it = subset.begin();
         tag_it != subset.end(); ++tag_it) {
      bookmark_counts[*tag_it] += 1;
    }
  }
  // Keep only the bookmarks that appeared in all the tags.
  std::set<const BookmarkNode*> common_bookmarks;
  for (std::map<const BookmarkNode*, size_t>::iterator it =
      bookmark_counts.begin(); it != bookmark_counts.end(); ++it) {
    if (it->second == tags.size())
      common_bookmarks.insert(it->first);
  }
  return common_bookmarks;
}

std::set<const BookmarkNode*> BookmarkTagModel::BookmarksForTag(
    const BookmarkTag& tag) {
  DCHECK(!tag.empty());
  return tag_to_bookmarks_[tag];
}

std::vector<BookmarkTag> BookmarkTagModel::TagsRelatedToTag(
    const BookmarkTag& tag) {
  DCHECK(loaded_);
  std::map<BookmarkTag, unsigned int> tags;

  if (tag.empty()) {
    // Returns all the tags.
    for (std::map<const BookmarkTag, std::set<const BookmarkNode*> >::iterator
        it = tag_to_bookmarks_.begin(); it != tag_to_bookmarks_.end(); ++it) {
      tags[it->first] = it->second.size();
    }
  } else {
    std::set<const BookmarkNode*> bookmarks(BookmarksForTag(tag));

    for (std::set<const BookmarkNode*>::iterator it = bookmarks.begin();
         it != bookmarks.end(); ++it) {
      const std::set<BookmarkTag>& subset(bookmark_to_tags_[*it]);
      for (std::set<BookmarkTag>::const_iterator tag_it = subset.begin();
           tag_it != subset.end(); ++tag_it) {
        tags[*tag_it] += 1;
      }
    }
    tags.erase(tag);  // A tag is not related to itself.
  }

  std::vector<BookmarkTag> sorted_tags;
  for (std::map<BookmarkTag, unsigned int>::iterator it = tags.begin();
       it != tags.end(); ++it) {
    sorted_tags.push_back(it->first);
  }
  std::sort(sorted_tags.begin(), sorted_tags.end(), TagComparator(tags));
  return sorted_tags;
}

// BookmarkModelObserver methods.

void BookmarkTagModel::Loaded(BookmarkModel* model, bool ids_reassigned) {
  Load();
}

void BookmarkTagModel::BookmarkModelBeingDeleted(BookmarkModel* model) {
  DCHECK(bookmark_model_);
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    BookmarkTagModelBeingDeleted(this));
  bookmark_model_ = NULL;
}

void BookmarkTagModel::BookmarkNodeMoved(BookmarkModel* model,
                                         const BookmarkNode* old_parent,
                                         int old_index,
                                         const BookmarkNode* new_parent,
                                         int new_index) {
  DCHECK(loaded_);
  const BookmarkNode* bookmark = new_parent->GetChild(new_index);
  if (bookmark->is_folder()) {
    ReloadDescendants(bookmark);
  } else if (bookmark->is_url()) {
    std::string encoded;
    if (!bookmark->GetMetaInfo(TAG_KEY, &encoded)) {
      // The bookmark moved and the system currently use its ancestors name as a
      // poor approximation for tags.
      FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                        OnWillChangeBookmarkTags(this, bookmark));
      RemoveBookmark(bookmark);
      LoadBookmark(bookmark);
      FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                        BookmarkTagsChanged(this, bookmark));
    }
  }
}

void BookmarkTagModel::BookmarkNodeAdded(BookmarkModel* model,
                                         const BookmarkNode* parent,
                                         int index) {
  DCHECK(loaded_);
  const BookmarkNode* bookmark = parent->GetChild(index);
  if (!bookmark->is_url())
    return;
  LoadBookmark(bookmark);

  if (!inhibit_change_notifications_)
    FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                      BookmarkNodeAdded(this, bookmark));
}

void BookmarkTagModel::OnWillRemoveBookmarks(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int old_index,
                                             const BookmarkNode* node) {
  DCHECK(loaded_);
  if (!node->is_url())
    return;
  RemoveBookmark(node);
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    OnWillRemoveBookmarks(this, node));
}

void BookmarkTagModel::BookmarkNodeRemoved(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int old_index,
                                           const BookmarkNode* node) {
  DCHECK(loaded_);
  if (!node->is_url())
    return;
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    BookmarkNodeRemoved(this, node));
}

void BookmarkTagModel::OnWillChangeBookmarkNode(BookmarkModel* model,
                                                const BookmarkNode* node) {
  DCHECK(loaded_);
  if (!node->is_url() || inhibit_change_notifications_)
    return;
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    OnWillChangeBookmarkNode(this, node));
}

void BookmarkTagModel::BookmarkNodeChanged(BookmarkModel* model,
                                           const BookmarkNode* node) {
  DCHECK(loaded_);
  if (node->is_folder()) {
    // A folder title changed. This may change the tags on all the descendants
    // still using the default tag list of all ancestors.
    ReloadDescendants(node);
  } else if (node->is_url()) {
    if (!inhibit_change_notifications_)
      FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                        BookmarkNodeChanged(this, node));
  }
}

void BookmarkTagModel::OnWillChangeBookmarkMetaInfo(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  DCHECK(loaded_);
  if (!node->is_url() || inhibit_change_notifications_)
    return;
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    OnWillChangeBookmarkTags(this, node));
}

void BookmarkTagModel::BookmarkMetaInfoChanged(BookmarkModel* model,
                                               const BookmarkNode* node) {
  DCHECK(loaded_);
  if (!node->is_url())
    return;
  RemoveBookmark(node);
  LoadBookmark(node);
  if (!inhibit_change_notifications_)
    FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                      BookmarkTagsChanged(this, node));
}

void BookmarkTagModel::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  DCHECK(loaded_);
  if (!node->is_url())
    return;
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    BookmarkNodeFaviconChanged(this, node));
}

void BookmarkTagModel::OnWillReorderBookmarkNode(BookmarkModel* model,
                                                 const BookmarkNode* node) {
  // This model doesn't care.
}

void BookmarkTagModel::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  // This model doesn't care.
}

void BookmarkTagModel::ExtensiveBookmarkChangesBeginning(BookmarkModel* model) {
  DCHECK(loaded_);
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    ExtensiveBookmarkChangesBeginning(this));
}

void BookmarkTagModel::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  DCHECK(loaded_);
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    ExtensiveBookmarkChangesEnded(this));
}

void BookmarkTagModel::OnWillRemoveAllBookmarks(BookmarkModel* model) {
  DCHECK(loaded_);
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    OnWillRemoveAllBookmarks(this));
}

void BookmarkTagModel::BookmarkAllNodesRemoved(BookmarkModel* model){
  DCHECK(loaded_);
  tag_to_bookmarks_.clear();
  bookmark_to_tags_.clear();
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    BookmarkAllNodesRemoved(this));
}

// Private methods.

void BookmarkTagModel::SetTagsOnBookmark(const std::set<BookmarkTag>& tags,
                                         const BookmarkNode* bookmark) {
  DCHECK(bookmark_model_);
  DCHECK(loaded_);

  // Build a ListValue.
  std::vector<BookmarkTag> tag_vector(tags.begin(), tags.end());
  base::ListValue list;
  list.AppendStrings(tag_vector);

  // Encodes it.
  std::string encoded;
  JSONStringValueSerializer serializer(&encoded);

  // Pushes it in the bookmark's metainfo. Even if the tag list is empty the
  // empty list must be put on the node to avoid reverting to the tag list
  // derived from the hierarchy.
  // The internal caches of the BookmarkTagModel are updated when the
  // notification from the BookmarkModel is received.
  serializer.Serialize(list);
  bookmark_model_->SetNodeMetaInfo(bookmark, TAG_KEY, encoded);
}

void BookmarkTagModel::Load() {
  DCHECK(bookmark_model_);
  DCHECK(!loaded_);
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      bookmark_model_->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url())
      LoadBookmark(bookmark);
  }
  loaded_ = true;
  FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                    Loaded(this));
}

void BookmarkTagModel::ReloadDescendants(const BookmarkNode* folder) {
  DCHECK(folder->is_folder());
  ExtensiveChanges scoped(ExtensiveChanges(this));
  ui::TreeNodeIterator<const BookmarkNode> iterator(folder);
  while (iterator.has_next()) {
    const BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url()) {
      FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                        OnWillChangeBookmarkTags(this, bookmark));
      RemoveBookmark(bookmark);
      LoadBookmark(bookmark);
      FOR_EACH_OBSERVER(BookmarkTagModelObserver, observers_,
                        BookmarkTagsChanged(this, bookmark));
    }
  }
}

void BookmarkTagModel::LoadBookmark(const BookmarkNode* bookmark) {
  DCHECK(bookmark_model_);
  DCHECK(bookmark->is_url());
  std::set<BookmarkTag> tags(ExtractTagsFromBookmark(bookmark));

  bookmark_to_tags_[bookmark] = tags;
  for (std::set<BookmarkTag>::iterator it = tags.begin();
       it != tags.end(); ++it) {
    tag_to_bookmarks_[*it].insert(bookmark);
  }
}

void BookmarkTagModel::RemoveBookmark(const BookmarkNode* bookmark) {
  DCHECK(bookmark_model_);
  DCHECK(bookmark->is_url());
  std::set<BookmarkTag> tags(bookmark_to_tags_[bookmark]);
  bookmark_to_tags_.erase(bookmark);

  for (std::set<BookmarkTag>::iterator it = tags.begin();
       it != tags.end(); ++it) {
    tag_to_bookmarks_[*it].erase(bookmark);
    // Remove the tags no longer used.
    if (!tag_to_bookmarks_[*it].size())
      tag_to_bookmarks_.erase(*it);
  }
}
