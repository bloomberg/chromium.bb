// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/protocol/sync.pb.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

void UpdateBookmarkSpecificsMetaInfo(
    const bookmarks::BookmarkNode::MetaInfoMap* metainfo_map,
    sync_pb::BookmarkSpecifics* bm_specifics) {
  // TODO(crbug.com/516866): update the implementation to be similar to the
  // directory implementation
  // https://cs.chromium.org/chromium/src/components/sync_bookmarks/bookmark_change_processor.cc?l=882&rcl=f38001d936d8b2abb5743e85cbc88c72746ae3d2
  for (const std::pair<std::string, std::string>& pair : *metainfo_map) {
    sync_pb::MetaInfo* meta_info = bm_specifics->add_meta_info();
    meta_info->set_key(pair.first);
    meta_info->set_value(pair.second);
  }
}

// Metainfo entries in |specifics| must have unique keys.
bookmarks::BookmarkNode::MetaInfoMap GetBookmarkMetaInfo(
    const sync_pb::BookmarkSpecifics& specifics) {
  bookmarks::BookmarkNode::MetaInfoMap meta_info_map;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    meta_info_map[meta_info.key()] = meta_info.value();
  }
  DCHECK_EQ(static_cast<size_t>(specifics.meta_info_size()),
            meta_info_map.size());
  return meta_info_map;
}

}  // namespace

sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(node->url().spec());
  // TODO(crbug.com/516866): Set the favicon.
  bm_specifics->set_title(base::UTF16ToUTF8(node->GetTitle()));
  bm_specifics->set_creation_time_us(
      node->date_added().ToDeltaSinceWindowsEpoch().InMicroseconds());

  bm_specifics->set_icon_url(node->icon_url() ? node->icon_url()->spec()
                                              : std::string());
  if (node->GetMetaInfoMap()) {
    UpdateBookmarkSpecificsMetaInfo(node->GetMetaInfoMap(), bm_specifics);
  }
  return specifics;
}

const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,

    int index,
    bool is_folder,
    bookmarks::BookmarkModel* model) {
  DCHECK(parent);
  DCHECK(model);

  bookmarks::BookmarkNode::MetaInfoMap metainfo =
      GetBookmarkMetaInfo(specifics);
  if (is_folder) {
    return model->AddFolderWithMetaInfo(
        parent, index, base::UTF8ToUTF16(specifics.title()), &metainfo);
  }
  const int64_t create_time_us = specifics.creation_time_us();
  base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
      // Use FromDeltaSinceWindowsEpoch because create_time_us has
      // always used the Windows epoch.
      base::TimeDelta::FromMicroseconds(create_time_us));
  return model->AddURLWithCreationTimeAndMetaInfo(
      parent, index, base::UTF8ToUTF16(specifics.title()),
      GURL(specifics.url()), create_time, &metainfo);
  // TODO(crbug.com/516866): Add the favicon related code.
}

void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model) {
  if (!node->is_folder()) {
    model->SetURL(node, GURL(specifics.url()));
  }

  model->SetTitle(node, base::UTF8ToUTF16(specifics.title()));
  // TODO(crbug.com/516866): Add the favicon related code.
  model->SetNodeMetaInfoMap(node, GetBookmarkMetaInfo(specifics));
}

bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics,
                              bool is_folder) {
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid bookmark: empty specifics.";
    return false;
  }
  if (!is_folder && !GURL(specifics.url()).is_valid()) {
    DLOG(ERROR) << "Invalid bookmark: invalid url in the specifics.";
    return false;
  }
  // Verify all keys in meta_info are unique.
  std::unordered_set<base::StringPiece, base::StringPieceHash> keys;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    if (!keys.insert(meta_info.key()).second) {
      DLOG(ERROR) << "Invalid bookmark: keys in meta_info aren't unique.";
      return false;
    }
  }
  return true;
}

}  // namespace sync_bookmarks
