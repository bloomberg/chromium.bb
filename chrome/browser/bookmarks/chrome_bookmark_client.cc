// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/managed/managed_bookmark_util.h"
#include "components/favicon/core/favicon_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"

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

bool ChromeBookmarkClient::PreferTouchIcon() {
  return false;
}

base::CancelableTaskTracker::TaskId
ChromeBookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::IconType type,
    const favicon_base::FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  return favicon::GetFaviconImageForPageURL(
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      page_url, type, callback, tracker);
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
  DCHECK(bookmarks::IsPermanentNode(node, managed_bookmark_service_));
  if (bookmarks::IsManagedNode(node, managed_bookmark_service_))
    return false;
#if defined(OS_ANDROID)
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
