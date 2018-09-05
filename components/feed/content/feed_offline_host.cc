// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include "url/gurl.h"

namespace feed {

FeedOfflineHost::FeedOfflineHost(
    offline_pages::OfflinePageModel* offline_page_model,
    offline_pages::PrefetchService* prefetch_service,
    base::RepeatingClosure on_suggestion_consumed,
    base::RepeatingClosure on_suggestions_shown)
    : offline_page_model_(offline_page_model),
      prefetch_service_(prefetch_service),
      on_suggestion_consumed_(on_suggestion_consumed),
      on_suggestions_shown_(on_suggestions_shown),
      weak_ptr_factory_(this) {
  DCHECK(offline_page_model_);
  DCHECK(prefetch_service_);
  DCHECK(!on_suggestion_consumed_.is_null());
  DCHECK(!on_suggestions_shown_.is_null());
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
  on_suggestion_consumed_.Run();
}

void FeedOfflineHost::ReportArticleViewed(GURL article_url) {
  on_suggestions_shown_.Run();
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
