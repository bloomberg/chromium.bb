// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/history_client_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/history/core/browser/history_service.h"
#include "ios/chrome/browser/history/history_backend_client_impl.h"
#include "ios/chrome/browser/history/history_utils.h"
#include "url/gurl.h"

HistoryClientImpl::HistoryClientImpl(bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  if (bookmark_model_)
    bookmark_model_->AddObserver(this);
}

HistoryClientImpl::~HistoryClientImpl() {
  StopObservingBookmarkModel();
}

void HistoryClientImpl::StopObservingBookmarkModel() {
  if (!bookmark_model_)
    return;
  bookmark_model_->RemoveObserver(this);
  bookmark_model_ = nullptr;
}

void HistoryClientImpl::OnHistoryServiceCreated(
    history::HistoryService* history_service) {
  if (bookmark_model_) {
    on_bookmarks_removed_ =
        base::Bind(&history::HistoryService::URLsNoLongerBookmarked,
                   base::Unretained(history_service));
    favicons_changed_subscription_ =
        history_service->AddFaviconsChangedCallback(
            base::Bind(&bookmarks::BookmarkModel::OnFaviconsChanged,
                       base::Unretained(bookmark_model_)));
  }
}

void HistoryClientImpl::Shutdown() {
  favicons_changed_subscription_.reset();
  StopObservingBookmarkModel();
}

bool HistoryClientImpl::CanAddURL(const GURL& url) {
  return ios::CanAddURLToHistory(url);
}

void HistoryClientImpl::NotifyProfileError(sql::InitStatus init_status,
                                           const std::string& diagnostics) {}

std::unique_ptr<history::HistoryBackendClient>
HistoryClientImpl::CreateBackendClient() {
  return std::make_unique<HistoryBackendClientImpl>(
      bookmark_model_ ? bookmark_model_->model_loader() : nullptr);
}

void HistoryClientImpl::BookmarkModelChanged() {
}

void HistoryClientImpl::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  DCHECK_EQ(model, bookmark_model_);
  StopObservingBookmarkModel();
}

void HistoryClientImpl::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  if (on_bookmarks_removed_)
    on_bookmarks_removed_.Run(no_longer_bookmarked);
}

void HistoryClientImpl::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  if (on_bookmarks_removed_)
    on_bookmarks_removed_.Run(removed_urls);
}
