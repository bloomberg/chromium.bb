// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include <utility>

#include "base/bind.h"
#include "base/hash.h"
#include "base/metrics/histogram_macros.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "url/gurl.h"

namespace feed {

namespace {

// |url| is always set. Sometimes |original_url| is set. If |original_url| is
// set it is returned by this method, otherwise fall back to |url|.
const GURL& PreferOriginal(const offline_pages::OfflinePageItem& item) {
  return item.original_url.is_empty() ? item.url : item.original_url;
}

// Aggregates multiple callbacks from OfflinePageModel, storing the offline url.
// When all callbacks have been invoked, tracked by ref counting, then
// |on_completeion_| is finally invoked, sending all results together.
class CallbackAggregator : public base::RefCounted<CallbackAggregator> {
 public:
  using ReportStatusCallback =
      base::OnceCallback<void(std::vector<std::string>)>;
  using CacheIdCallback =
      base::RepeatingCallback<void(const std::string&, int64_t)>;

  CallbackAggregator(ReportStatusCallback on_completion,
                     CacheIdCallback on_each_result)
      : on_completion_(std::move(on_completion)),
        on_each_result_(std::move(on_each_result)),
        start_time_(base::Time::Now()) {}

  void OnGetPages(const std::vector<offline_pages::OfflinePageItem>& pages) {
    if (!pages.empty()) {
      offline_pages::OfflinePageItem newest =
          *std::max_element(pages.begin(), pages.end(), [](auto lhs, auto rhs) {
            return lhs.creation_time < rhs.creation_time;
          });
      const std::string& url = PreferOriginal(newest).spec();
      urls_.push_back(url);
      on_each_result_.Run(url, newest.offline_id);
    }
  }

 private:
  friend class base::RefCounted<CallbackAggregator>;

  ~CallbackAggregator() {
    base::TimeDelta duration = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_TIMES("Feed.Offline.GetStatusDuration", duration);
    std::move(on_completion_).Run(std::move(urls_));
  }

  // To be called once all callbacks are run or destroyed.
  ReportStatusCallback on_completion_;

  // The urls of the offlined pages seen so far. Ultimately will be given to
  // |on_completeion_|.
  std::vector<std::string> urls_;

  // Is not run if there are no results for a given url.
  CacheIdCallback on_each_result_;

  // The time the aggregator was created, before any requests were sent to the
  // OfflinePageModel.
  base::Time start_time_;
};

}  //  namespace

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

base::Optional<int64_t> FeedOfflineHost::GetOfflineId(const std::string& url) {
  auto iter = url_hash_to_id_.find(base::Hash(url));
  return iter == url_hash_to_id_.end() ? base::Optional<int64_t>()
                                       : base::Optional<int64_t>(iter->second);
}

void FeedOfflineHost::GetOfflineStatus(
    std::vector<std::string> urls,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  UMA_HISTOGRAM_EXACT_LINEAR("Feed.Offline.GetStatusCount", urls.size(), 50);

  scoped_refptr<CallbackAggregator> aggregator =
      base::MakeRefCounted<CallbackAggregator>(
          std::move(callback),
          base::BindRepeating(&FeedOfflineHost::CacheOfflinePageAndId,
                              weak_ptr_factory_.GetWeakPtr()));

  for (std::string url : urls) {
    offline_page_model_->GetPagesByURL(
        GURL(url), base::BindOnce(&CallbackAggregator::OnGetPages, aggregator));
  }
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

void FeedOfflineHost::CacheOfflinePageAndId(const std::string& url,
                                            int64_t id) {
  url_hash_to_id_[base::Hash(url)] = id;
}

}  // namespace feed
