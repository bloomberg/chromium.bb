// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"
#include "ui/base/l10n/l10n_util.h"

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

ChromeBookmarkClient::ChromeBookmarkClient(
    Profile* profile,
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : profile_(profile), managed_bookmark_service_(managed_bookmark_service) {}

ChromeBookmarkClient::~ChromeBookmarkClient() {
}

void ChromeBookmarkClient::Init(bookmarks::BookmarkModel* model) {
  if (managed_bookmark_service_)
    managed_bookmark_service_->BookmarkModelCreated(model);
}

void ChromeBookmarkClient::Shutdown() {
  managed_bookmark_service_ = nullptr;
  BookmarkClient::Shutdown();
}

bool ChromeBookmarkClient::PreferTouchIcon() {
#if !defined(OS_IOS)
  return false;
#else
  return true;
#endif
}

base::CancelableTaskTracker::TaskId
ChromeBookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::IconType type,
    const favicon_base::FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return base::CancelableTaskTracker::kBadTaskId;
  if (type == favicon_base::FAVICON) {
    return favicon_service->GetFaviconImageForPageURL(
        page_url, callback, tracker);
  } else {
    return favicon_service->GetRawFaviconForPageURL(
        page_url,
        type,
        0,
        base::Bind(&RunCallbackWithImage, callback),
        tracker);
  }
}

bool ChromeBookmarkClient::SupportsTypedCountForNodes() {
  return true;
}

void ChromeBookmarkClient::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  history::URLDatabase* url_db =
      history_service ? history_service->InMemoryDatabase() : nullptr;
  for (NodeSet::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
    int typed_count = 0;

    // If |url_db| is the InMemoryDatabase, it might not cache all URLRows, but
    // it guarantees to contain those with |typed_count| > 0. Thus, if we cannot
    // fetch the URLRow, it is safe to assume that its |typed_count| is 0.
    history::URLRow url;
    if (url_db && url_db->GetRowForURL((*i)->url(), &url))
      typed_count = url.typed_count();

    NodeTypedCountPair pair(*i, typed_count);
    node_typed_count_pairs->push_back(pair);
  }
}

bool ChromeBookmarkClient::IsPermanentNodeVisible(
    const bookmarks::BookmarkPermanentNode* node) {
  DCHECK(IsPermanentNode(node, managed_bookmark_service_));
  if (managed_bookmark_service_ &&
      (node == managed_bookmark_service_->managed_node() ||
       node == managed_bookmark_service_->supervised_node())) {
    return false;
  }
#if defined(OS_IOS) || defined(OS_ANDROID)
  return node->type() == bookmarks::BookmarkNode::MOBILE;
#else
  return node->type() != bookmarks::BookmarkNode::MOBILE;
#endif
}

void ChromeBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
  content::RecordAction(action);
}

bookmarks::LoadExtraCallback ChromeBookmarkClient::GetLoadExtraNodesCallback() {
  if (!managed_bookmark_service_)
    return bookmarks::LoadExtraCallback();

  return managed_bookmark_service_->GetLoadExtraNodesCallback();
}

bool ChromeBookmarkClient::CanSetPermanentNodeTitle(
    const bookmarks::BookmarkNode* permanent_node) {
  return !managed_bookmark_service_
             ? true
             : managed_bookmark_service_->CanSetPermanentNodeTitle(
                   permanent_node);
}

bool ChromeBookmarkClient::CanSyncNode(const bookmarks::BookmarkNode* node) {
  return !managed_bookmark_service_
             ? true
             : managed_bookmark_service_->CanSyncNode(node);
}

bool ChromeBookmarkClient::CanBeEditedByUser(
    const bookmarks::BookmarkNode* node) {
  return !managed_bookmark_service_
             ? true
             : managed_bookmark_service_->CanBeEditedByUser(node);
}
