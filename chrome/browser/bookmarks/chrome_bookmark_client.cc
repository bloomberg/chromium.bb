// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_changed_details.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"

namespace {

void NotifyHistoryOfRemovedURLs(Profile* profile,
                                const std::set<GURL>& removed_urls) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->URLsNoLongerBookmarked(removed_urls);
}

}  // namespace

ChromeBookmarkClient::ChromeBookmarkClient(Profile* profile, bool index_urls)
    : profile_(profile),
      model_(new BookmarkModel(this, index_urls)) {
  model_->AddObserver(this);
  // Listen for changes to favicons so that we can update the favicon of the
  // node appropriately.
  registrar_.Add(this,
                 chrome::NOTIFICATION_FAVICON_CHANGED,
                 content::Source<Profile>(profile_));
}

ChromeBookmarkClient::~ChromeBookmarkClient() {
  model_->RemoveObserver(this);

  registrar_.RemoveAll();
}

bool ChromeBookmarkClient::PreferTouchIcon() {
#if !defined(OS_IOS)
  return false;
#else
  return true;
#endif
}

base::CancelableTaskTracker::TaskId ChromeBookmarkClient::GetFaviconImageForURL(
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return base::CancelableTaskTracker::kBadTaskId;
  return favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(
          page_url, icon_types, desired_size_in_dip),
      callback,
      tracker);
}

bool ChromeBookmarkClient::SupportsTypedCountForNodes() {
  return true;
}

void ChromeBookmarkClient::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  history::URLDatabase* url_db =
      history_service ? history_service->InMemoryDatabase() : NULL;
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

void ChromeBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
  content::RecordAction(action);
}

bool ChromeBookmarkClient::IsPermanentNodeVisible(int node_type) {
  DCHECK(node_type == BookmarkNode::BOOKMARK_BAR ||
         node_type == BookmarkNode::OTHER_NODE ||
         node_type == BookmarkNode::MOBILE);
#if !defined(OS_IOS)
  return node_type != BookmarkNode::MOBILE;
#else
  return node_type == BookmarkNode::MOBILE;
#endif
}

void ChromeBookmarkClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FAVICON_CHANGED: {
      content::Details<FaviconChangedDetails> favicon_details(details);
      model_->OnFaviconChanged(favicon_details->urls);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void ChromeBookmarkClient::Shutdown() {
  model_->Shutdown();
}

void ChromeBookmarkClient::BookmarkModelChanged() {
}

void ChromeBookmarkClient::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  NotifyHistoryOfRemovedURLs(profile_, removed_urls);
}

void ChromeBookmarkClient::BookmarkAllNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  NotifyHistoryOfRemovedURLs(profile_, removed_urls);
}
