// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_client_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/web/public/user_metrics.h"

namespace {

void RunCallbackWithImage(
    const favicon_base::FaviconImageCallback& callback,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  favicon_base::FaviconImageResult result;
  if (bitmap_result.is_valid()) {
    result.image = gfx::Image::CreateFrom1xPNGBytes(
        bitmap_result.bitmap_data->front(), bitmap_result.bitmap_data->size());
    result.icon_url = bitmap_result.icon_url;
    callback.Run(result);
    return;
  }
  callback.Run(result);
}

bool IsPermanentNode(
    const bookmarks::BookmarkPermanentNode* node,
    bookmarks::ManagedBookmarkService* managed_bookmark_service) {
  bookmarks::BookmarkNode::Type type = node->type();
  if (type == bookmarks::BookmarkNode::BOOKMARK_BAR ||
      type == bookmarks::BookmarkNode::OTHER_NODE ||
      type == bookmarks::BookmarkNode::MOBILE) {
    return true;
  }

  if (!managed_bookmark_service)
    return false;

  return node == managed_bookmark_service->managed_node() ||
         node == managed_bookmark_service->supervised_node();
}

}  // namespace

BookmarkClientImpl::BookmarkClientImpl(
    ios::ChromeBrowserState* browser_state,
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : browser_state_(browser_state),
      managed_bookmark_service_(managed_bookmark_service) {}

BookmarkClientImpl::~BookmarkClientImpl() {}

void BookmarkClientImpl::Init(bookmarks::BookmarkModel* bookmark_model) {
  if (managed_bookmark_service_)
    managed_bookmark_service_->BookmarkModelCreated(bookmark_model);
}

void BookmarkClientImpl::Shutdown() {
  managed_bookmark_service_ = nullptr;
  bookmarks::BookmarkClient::Shutdown();
}

bool BookmarkClientImpl::PreferTouchIcon() {
  return true;
}

base::CancelableTaskTracker::TaskId
BookmarkClientImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::IconType type,
    const favicon_base::FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  favicon::FaviconService* favicon_service =
      ios::FaviconServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return base::CancelableTaskTracker::kBadTaskId;
  if (type == favicon_base::FAVICON) {
    return favicon_service->GetFaviconImageForPageURL(page_url, callback,
                                                      tracker);
  } else {
    return favicon_service->GetRawFaviconForPageURL(
        page_url, type, 0, base::Bind(&RunCallbackWithImage, callback),
        tracker);
  }
}

bool BookmarkClientImpl::SupportsTypedCountForNodes() {
  return true;
}

void BookmarkClientImpl::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  history::URLDatabase* url_db =
      history_service ? history_service->InMemoryDatabase() : nullptr;
  for (const auto& node : nodes) {
    // If |url_db| is the InMemoryDatabase, it might not cache all URLRows, but
    // it guarantees to contain those with |typed_count| > 0. Thus, if fetching
    // the URLRow fails, it is safe to assume that its |typed_count| is 0.
    int typed_count = 0;
    history::URLRow url;
    if (url_db && url_db->GetRowForURL(node->url(), &url))
      typed_count = url.typed_count();

    NodeTypedCountPair pair(node, typed_count);
    node_typed_count_pairs->push_back(pair);
  }
}

bool BookmarkClientImpl::IsPermanentNodeVisible(
    const bookmarks::BookmarkPermanentNode* node) {
  DCHECK(IsPermanentNode(node, managed_bookmark_service_));
  if (managed_bookmark_service_) {
    if (node == managed_bookmark_service_->managed_node() ||
        node == managed_bookmark_service_->supervised_node()) {
      return false;
    }
  }
  return node->type() == bookmarks::BookmarkNode::MOBILE;
}

void BookmarkClientImpl::RecordAction(const base::UserMetricsAction& action) {
  web::RecordAction(action);
}

bookmarks::LoadExtraCallback BookmarkClientImpl::GetLoadExtraNodesCallback() {
  if (!managed_bookmark_service_)
    return bookmarks::LoadExtraCallback();

  return managed_bookmark_service_->GetLoadExtraNodesCallback();
}

bool BookmarkClientImpl::CanSetPermanentNodeTitle(
    const bookmarks::BookmarkNode* permanent_node) {
  return !managed_bookmark_service_
             ? true
             : managed_bookmark_service_->CanSetPermanentNodeTitle(
                   permanent_node);
}

bool BookmarkClientImpl::CanSyncNode(const bookmarks::BookmarkNode* node) {
  return !managed_bookmark_service_
             ? true
             : managed_bookmark_service_->CanSyncNode(node);
}

bool BookmarkClientImpl::CanBeEditedByUser(
    const bookmarks::BookmarkNode* node) {
  return !managed_bookmark_service_
             ? true
             : managed_bookmark_service_->CanBeEditedByUser(node);
}
