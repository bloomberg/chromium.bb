// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"

#include <iomanip>
#include <sstream>

#include "base/base64.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/rand_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model_observer.h"
#include "components/enhanced_bookmarks/proto/metadata.pb.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

namespace {
const char* kBookmarkBarId = "f_bookmarks_bar";

const char* kIdKey = "stars.id";
const char* kImageDataKey = "stars.imageData";
const char* kNoteKey = "stars.note";
const char* kOldIdKey = "stars.oldId";
const char* kPageDataKey = "stars.pageData";
const char* kVersionKey = "stars.version";

const char* kBookmarkPrefix = "ebc_";

// Helper method for working with bookmark metainfo.
std::string DataForMetaInfoField(const BookmarkNode* node,
                                 const std::string& field) {
  std::string value;
  if (!node->GetMetaInfo(field, &value))
    return std::string();

  std::string decoded;
  if (!base::Base64Decode(value, &decoded))
    return std::string();

  return decoded;
}

// Helper method for working with ImageData_ImageInfo.
bool PopulateImageData(const image::collections::ImageData_ImageInfo& info,
                       GURL* out_url,
                       int* width,
                       int* height) {
  if (!info.has_url() || !info.has_width() || !info.has_height())
    return false;

  GURL url(info.url());
  if (!url.is_valid())
    return false;

  *out_url = url;
  *width = info.width();
  *height = info.height();
  return true;
}

// Generate a random remote id, with a prefix that depends on whether the node
// is a folder or a bookmark.
std::string GenerateRemoteId() {
  std::stringstream random_id;
  random_id << kBookmarkPrefix;

  // Generate 32 digit hex string random suffix.
  random_id << std::hex << std::setfill('0') << std::setw(16);
  random_id << base::RandUint64() << base::RandUint64();
  return random_id.str();
}
}  // namespace

namespace enhanced_bookmarks {

EnhancedBookmarkModel::EnhancedBookmarkModel(BookmarkModel* bookmark_model,
                                             const std::string& version)
    : bookmark_model_(bookmark_model),
      loaded_(false),
      weak_ptr_factory_(this),
      version_(version) {
  bookmark_model_->AddObserver(this);
  if (bookmark_model_->loaded()) {
    InitializeIdMap();
    loaded_ = true;
  }
}

EnhancedBookmarkModel::~EnhancedBookmarkModel() {
}

void EnhancedBookmarkModel::Shutdown() {
  FOR_EACH_OBSERVER(EnhancedBookmarkModelObserver,
                    observers_,
                    EnhancedBookmarkModelShuttingDown());
  weak_ptr_factory_.InvalidateWeakPtrs();
  bookmark_model_->RemoveObserver(this);
  bookmark_model_ = NULL;
}

void EnhancedBookmarkModel::AddObserver(
    EnhancedBookmarkModelObserver* observer) {
  observers_.AddObserver(observer);
}

void EnhancedBookmarkModel::RemoveObserver(
    EnhancedBookmarkModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

// Moves |node| to |new_parent| and inserts it at the given |index|.
void EnhancedBookmarkModel::Move(const BookmarkNode* node,
                                 const BookmarkNode* new_parent,
                                 int index) {
  bookmark_model_->Move(node, new_parent, index);
}

// Adds a new folder node at the specified position.
const BookmarkNode* EnhancedBookmarkModel::AddFolder(
    const BookmarkNode* parent,
    int index,
    const base::string16& title) {
  return bookmark_model_->AddFolder(parent, index, title);
}

// Adds a url at the specified position.
const BookmarkNode* EnhancedBookmarkModel::AddURL(
    const BookmarkNode* parent,
    int index,
    const base::string16& title,
    const GURL& url,
    const base::Time& creation_time) {
  BookmarkNode::MetaInfoMap meta_info;
  meta_info[kIdKey] = GenerateRemoteId();
  return bookmark_model_->AddURLWithCreationTimeAndMetaInfo(
      parent, index, title, url, creation_time, &meta_info);
}

std::string EnhancedBookmarkModel::GetRemoteId(const BookmarkNode* node) {
  if (node == bookmark_model_->bookmark_bar_node())
    return kBookmarkBarId;

  std::string id;
  if (!node->GetMetaInfo(kIdKey, &id))
    return std::string();
  return id;
}

const BookmarkNode* EnhancedBookmarkModel::BookmarkForRemoteId(
    const std::string& remote_id) {
  IdToNodeMap::iterator it = id_map_.find(remote_id);
  if (it != id_map_.end())
    return it->second;
  return NULL;
}

void EnhancedBookmarkModel::SetDescription(const BookmarkNode* node,
                                           const std::string& description) {
  SetMetaInfo(node, kNoteKey, description);
}

std::string EnhancedBookmarkModel::GetDescription(const BookmarkNode* node) {
  // First, look for a custom note set by the user.
  std::string description;
  if (node->GetMetaInfo(kNoteKey, &description) && !description.empty())
    return description;

  // If none are present, return the snippet.
  return GetSnippet(node);
}

bool EnhancedBookmarkModel::SetOriginalImage(const BookmarkNode* node,
                                             const GURL& url,
                                             int width,
                                             int height) {
  DCHECK(node->is_url());
  DCHECK(url.is_valid());

  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  image::collections::ImageData data;

  // Try to populate the imageData with the existing data.
  if (decoded != "") {
    // If the parsing fails, something is wrong. Immediately fail.
    bool result = data.ParseFromString(decoded);
    if (!result)
      return false;
  }

  scoped_ptr<image::collections::ImageData_ImageInfo> info(
      new image::collections::ImageData_ImageInfo);
  info->set_url(url.spec());
  info->set_width(width);
  info->set_height(height);
  data.set_allocated_original_info(info.release());

  std::string output;
  bool result = data.SerializePartialToString(&output);
  if (!result)
    return false;

  std::string encoded;
  base::Base64Encode(output, &encoded);
  SetMetaInfo(node, kImageDataKey, encoded);
  return true;
}

bool EnhancedBookmarkModel::GetOriginalImage(const BookmarkNode* node,
                                             GURL* url,
                                             int* width,
                                             int* height) {
  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  if (decoded == "")
    return false;

  image::collections::ImageData data;
  bool result = data.ParseFromString(decoded);
  if (!result)
    return false;

  if (!data.has_original_info())
    return false;

  return PopulateImageData(data.original_info(), url, width, height);
}

bool EnhancedBookmarkModel::GetThumbnailImage(const BookmarkNode* node,
                                              GURL* url,
                                              int* width,
                                              int* height) {
  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  if (decoded == "")
    return false;

  image::collections::ImageData data;
  bool result = data.ParseFromString(decoded);
  if (!result)
    return false;

  if (!data.has_thumbnail_info())
    return false;

  return PopulateImageData(data.thumbnail_info(), url, width, height);
}

std::string EnhancedBookmarkModel::GetSnippet(const BookmarkNode* node) {
  std::string decoded(DataForMetaInfoField(node, kPageDataKey));
  if (decoded.empty())
    return decoded;

  image::collections::PageData data;
  bool result = data.ParseFromString(decoded);
  if (!result)
    return std::string();

  return data.snippet();
}

void EnhancedBookmarkModel::SetVersionSuffix(
    const std::string& version_suffix) {
  version_suffix_ = version_suffix;
}

void EnhancedBookmarkModel::BookmarkModelChanged() {
}

void EnhancedBookmarkModel::BookmarkModelLoaded(BookmarkModel* model,
                                                bool ids_reassigned) {
  InitializeIdMap();
  FOR_EACH_OBSERVER(
      EnhancedBookmarkModelObserver, observers_, EnhancedBookmarkModelLoaded());
}

void EnhancedBookmarkModel::BookmarkNodeAdded(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              int index) {
  const BookmarkNode* node = parent->GetChild(index);
  AddToIdMap(node);
  ScheduleResetDuplicateRemoteIds();
  FOR_EACH_OBSERVER(
      EnhancedBookmarkModelObserver, observers_, EnhancedBookmarkAdded(node));
}

void EnhancedBookmarkModel::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  std::string remote_id = GetRemoteId(node);
  id_map_.erase(remote_id);
  FOR_EACH_OBSERVER(
      EnhancedBookmarkModelObserver, observers_, EnhancedBookmarkRemoved(node));
}

void EnhancedBookmarkModel::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  prev_remote_id_ = GetRemoteId(node);
}

void EnhancedBookmarkModel::BookmarkMetaInfoChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  std::string remote_id = GetRemoteId(node);
  if (remote_id != prev_remote_id_) {
    id_map_.erase(prev_remote_id_);
    if (!remote_id.empty()) {
      AddToIdMap(node);
      ScheduleResetDuplicateRemoteIds();
    }
    FOR_EACH_OBSERVER(
        EnhancedBookmarkModelObserver,
        observers_,
        EnhancedBookmarkRemoteIdChanged(node, prev_remote_id_, remote_id));
  }
}

void EnhancedBookmarkModel::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  id_map_.clear();
  // Re-initialize so non-user nodes with remote ids are present in the map.
  InitializeIdMap();
  FOR_EACH_OBSERVER(EnhancedBookmarkModelObserver,
                    observers_,
                    EnhancedBookmarkAllUserNodesRemoved());
}

void EnhancedBookmarkModel::InitializeIdMap() {
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      bookmark_model_->root_node());
  while (iterator.has_next()) {
    AddToIdMap(iterator.Next());
  }
  ScheduleResetDuplicateRemoteIds();
}

void EnhancedBookmarkModel::AddToIdMap(const BookmarkNode* node) {
  std::string remote_id = GetRemoteId(node);
  if (remote_id.empty())
    return;

  // Try to insert the node.
  std::pair<IdToNodeMap::iterator, bool> result =
      id_map_.insert(make_pair(remote_id, node));
  if (!result.second) {
    // Some node already had the same remote id, so add both nodes to the
    // to-be-reset set.
    nodes_to_reset_[result.first->second] = remote_id;
    nodes_to_reset_[node] = remote_id;
  }
}

void EnhancedBookmarkModel::ScheduleResetDuplicateRemoteIds() {
  if (!nodes_to_reset_.empty()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&EnhancedBookmarkModel::ResetDuplicateRemoteIds,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void EnhancedBookmarkModel::ResetDuplicateRemoteIds() {
  for (NodeToIdMap::iterator it = nodes_to_reset_.begin();
       it != nodes_to_reset_.end();
       ++it) {
    BookmarkNode::MetaInfoMap meta_info;
    meta_info[kIdKey] = "";
    meta_info[kOldIdKey] = it->second;
    SetMultipleMetaInfo(it->first, meta_info);
  }
  nodes_to_reset_.clear();
}

void EnhancedBookmarkModel::SetMetaInfo(const BookmarkNode* node,
                                        const std::string& field,
                                        const std::string& value) {
  DCHECK(!bookmark_model_->is_permanent_node(node));

  BookmarkNode::MetaInfoMap meta_info;
  const BookmarkNode::MetaInfoMap* old_meta_info = node->GetMetaInfoMap();
  if (old_meta_info)
    meta_info.insert(old_meta_info->begin(), old_meta_info->end());

  // Don't update anything if the value to set is already there.
  BookmarkNode::MetaInfoMap::iterator it = meta_info.find(field);
  if (it != meta_info.end() && it->second == value)
    return;

  meta_info[field] = value;
  meta_info[kVersionKey] = GetVersionString();
  bookmark_model_->SetNodeMetaInfoMap(node, meta_info);
}

std::string EnhancedBookmarkModel::GetVersionString() {
  if (version_suffix_.empty())
    return version_;
  return version_ + '/' + version_suffix_;
}

void EnhancedBookmarkModel::SetMultipleMetaInfo(
    const BookmarkNode* node,
    BookmarkNode::MetaInfoMap meta_info) {
  DCHECK(!bookmark_model_->is_permanent_node(node));

  // Don't update anything if every value is already set correctly.
  if (node->GetMetaInfoMap()) {
    bool changed = false;
    const BookmarkNode::MetaInfoMap* old_meta_info = node->GetMetaInfoMap();
    for (BookmarkNode::MetaInfoMap::iterator it = meta_info.begin();
         it != meta_info.end();
         ++it) {
      BookmarkNode::MetaInfoMap::const_iterator old_field =
          old_meta_info->find(it->first);
      if (old_field == old_meta_info->end() ||
          old_field->second != it->second) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return;

    // Fill in the values that aren't changing
    meta_info.insert(old_meta_info->begin(), old_meta_info->end());
  }

  meta_info[kVersionKey] = GetVersionString();
  bookmark_model_->SetNodeMetaInfoMap(node, meta_info);
}

bool EnhancedBookmarkModel::SetAllImages(const BookmarkNode* node,
                                         const GURL& image_url,
                                         int image_width,
                                         int image_height,
                                         const GURL& thumbnail_url,
                                         int thumbnail_width,
                                         int thumbnail_height) {
  DCHECK(node->is_url());
  DCHECK(image_url.is_valid() || image_url.is_empty());
  DCHECK(thumbnail_url.is_valid() || thumbnail_url.is_empty());
  std::string decoded(DataForMetaInfoField(node, kImageDataKey));
  image::collections::ImageData data;

  // Try to populate the imageData with the existing data.
  if (decoded != "") {
    // If the parsing fails, something is wrong. Immediately fail.
    bool result = data.ParseFromString(decoded);
    if (!result)
      return false;
  }

  if (image_url.is_empty()) {
    data.release_original_info();
  } else {
    // Regardless of whether an image info exists, we make a new one.
    // Intentially make a raw pointer.
    image::collections::ImageData_ImageInfo* info =
        new image::collections::ImageData_ImageInfo;
    info->set_url(image_url.spec());
    info->set_width(image_width);
    info->set_height(image_height);
    // This method consumes the raw pointer.
    data.set_allocated_original_info(info);
  }

  if (thumbnail_url.is_empty()) {
    data.release_thumbnail_info();
  } else {
    // Regardless of whether an image info exists, we make a new one.
    // Intentially make a raw pointer.
    image::collections::ImageData_ImageInfo* info =
        new image::collections::ImageData_ImageInfo;
    info->set_url(thumbnail_url.spec());
    info->set_width(thumbnail_width);
    info->set_height(thumbnail_height);
    // This method consumes the raw pointer.
    data.set_allocated_thumbnail_info(info);
  }
  std::string output;
  bool result = data.SerializePartialToString(&output);
  if (!result)
    return false;

  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, kImageDataKey, encoded);
  return true;
}

}  // namespace enhanced_bookmarks
