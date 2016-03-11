// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_bookmark_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

namespace {
void EmptyDeleteCallback(OfflinePageModel::DeletePageResult /* result */) {}
}

OfflinePageBookmarkBridge::OfflinePageBookmarkBridge(
    OfflinePageModel* model,
    bookmarks::BookmarkModel* bookmark_model)
    : offline_model_(model), bookmark_model_(bookmark_model) {}

void OfflinePageBookmarkBridge::BookmarkModelChanged() {}

void OfflinePageBookmarkBridge::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int index) {
  DCHECK_EQ(model, bookmark_model_);
  const bookmarks::BookmarkNode* node = parent->GetChild(index);
  DCHECK(node);
  ClientId client_id(BOOKMARK_NAMESPACE, base::Int64ToString(node->id()));
  std::vector<int64_t> ids = offline_model_->GetOfflineIdsForClientId(
      client_id, true /* include_deleted */);

  for (const auto& id : ids)
    offline_model_->UndoPageDeletion(id);
}

void OfflinePageBookmarkBridge::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  DCHECK_EQ(model, bookmark_model_);
  ClientId client_id;
  client_id.name_space = BOOKMARK_NAMESPACE;
  client_id.id = base::Int64ToString(node->id());
  std::vector<int64_t> ids =
      offline_model_->GetOfflineIdsForClientId(client_id);
  offline_model_->MarkPagesForDeletion(ids, base::Bind(&EmptyDeleteCallback));
}

void OfflinePageBookmarkBridge::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  DCHECK_EQ(model, bookmark_model_);
  // BookmarkNodeChanged could be triggered if title or URL gets changed. If
  // the latter, we need to invalidate the offline copy.
  ClientId client_id;
  client_id.name_space = BOOKMARK_NAMESPACE;
  client_id.id = base::Int64ToString(node->id());
  std::vector<int64_t> ids =
      offline_model_->GetOfflineIdsForClientId(client_id);
  std::vector<int64_t> ids_to_delete;
  for (const auto& id : ids) {
    const OfflinePageItem* page = offline_model_->GetPageByOfflineId(id);
    if (page != nullptr && page->url != node->url())
      ids_to_delete.push_back(id);
  }
  offline_model_->DeletePagesByOfflineId(ids_to_delete,
                                         base::Bind(&EmptyDeleteCallback));
}

void OfflinePageBookmarkBridge::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  DCHECK_EQ(model, bookmark_model_);
  delete this;
}

}  // namespace offline_pages
