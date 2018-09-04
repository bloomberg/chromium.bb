// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

namespace feed {

FeedOfflineHost::FeedOfflineHost(
    offline_pages::OfflinePageModel* offline_page_model,
    offline_pages::PrefetchService* prefetch_service,
    FeedSchedulerHost* feed_scheduler_host)
    : offline_page_model_(offline_page_model),
      prefetch_service_(prefetch_service),
      feed_scheduler_host_(feed_scheduler_host),
      weak_ptr_factory_(this) {
  DCHECK(offline_page_model_);
  DCHECK(prefetch_service_);
  DCHECK(feed_scheduler_host_);
}

FeedOfflineHost::~FeedOfflineHost() = default;

base::Optional<int64_t> FeedOfflineHost::GetOfflineId(std::string url) {
  return {};
}

void FeedOfflineHost::GetOfflineStatus(
    std::vector<std::string> urls,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  // TODO(skym): Call OfflinePageModel::GetPagesByURL() for each url.
}

void FeedOfflineHost::OnContentRemoved(std::vector<std::string> urls) {
  // TODO(skym): Call PrefetchService::RemoveSuggestion().
}

void FeedOfflineHost::OnNewContentReceived() {
  // TODO(skym): Call PrefetchService::NewSuggestionsAvailable().
}

void FeedOfflineHost::OnNoListeners() {
  // TODO(skym): Clear out local cache of offline data.
}

void FeedOfflineHost::GetCurrentArticleSuggestions(
    offline_pages::SuggestionsProvider::SuggestionCallback
        suggestions_callback) {
  // TODO(skym): Call into bridge callback.
}

void FeedOfflineHost::ReportArticleListViewed() {
  // TODO(skym): Call FeedSchedulerHost::OnSuggestionsShown().
}

void FeedOfflineHost::ReportArticleViewed(GURL article_url) {
  // TODO(skym): Call FeedSchedulerHost::OnSuggestionConsumed().
}

void FeedOfflineHost::OfflinePageModelLoaded(
    offline_pages::OfflinePageModel* model) {
  // Ignored.
}

void FeedOfflineHost::OfflinePageAdded(
    offline_pages::OfflinePageModel* model,
    const offline_pages::OfflinePageItem& added_page) {
  // TODO(skym): Call into bridge callback.
}

void FeedOfflineHost::OfflinePageDeleted(
    const offline_pages::OfflinePageModel::DeletedPageInfo& page_info) {
  // TODO(skym): Call into bridge callback.
}

}  // namespace feed
