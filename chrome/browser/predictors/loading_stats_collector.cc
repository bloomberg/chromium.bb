// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include <set>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"

namespace predictors {

namespace {

void ReportPredictionAccuracy(
    const ResourcePrefetchPredictor::Prediction& prediction,
    const PageRequestSummary& summary) {
  const std::vector<GURL>& predicted_urls = prediction.subresource_urls;
  if (predicted_urls.empty() || summary.subresource_requests.empty())
    return;

  std::set<GURL> predicted_urls_set(predicted_urls.begin(),
                                    predicted_urls.end());
  std::set<GURL> actual_urls_set;
  for (const auto& request_summary : summary.subresource_requests)
    actual_urls_set.emplace(request_summary.resource_url);

  size_t correctly_predicted_count = 0;
  for (const GURL& predicted_url : predicted_urls_set) {
    if (actual_urls_set.find(predicted_url) != actual_urls_set.end())
      correctly_predicted_count++;
  }

  size_t precision_percentage =
      (100 * correctly_predicted_count) / predicted_urls_set.size();
  size_t recall_percentage =
      (100 * correctly_predicted_count) / actual_urls_set.size();

  using RedirectStatus = ResourcePrefetchPredictor::RedirectStatus;
  RedirectStatus redirect_status;
  if (summary.main_frame_url == summary.initial_url) {
    // The actual navigation wasn't redirected.
    redirect_status = prediction.is_redirected
                          ? RedirectStatus::NO_REDIRECT_BUT_PREDICTED
                          : RedirectStatus::NO_REDIRECT;
  } else {
    if (prediction.is_redirected) {
      std::string main_frame_key = prediction.is_host
                                       ? summary.main_frame_url.host()
                                       : summary.main_frame_url.spec();
      redirect_status = main_frame_key == prediction.main_frame_key
                            ? RedirectStatus::REDIRECT_CORRECTLY_PREDICTED
                            : RedirectStatus::REDIRECT_WRONG_PREDICTED;
    } else {
      redirect_status = RedirectStatus::REDIRECT_NOT_PREDICTED;
    }
  }

  UMA_HISTOGRAM_PERCENTAGE(
      internal::kResourcePrefetchPredictorPrecisionHistogram,
      precision_percentage);
  UMA_HISTOGRAM_PERCENTAGE(internal::kResourcePrefetchPredictorRecallHistogram,
                           recall_percentage);
  UMA_HISTOGRAM_COUNTS_100(internal::kResourcePrefetchPredictorCountHistogram,
                           predicted_urls.size());
  UMA_HISTOGRAM_ENUMERATION(
      internal::kResourcePrefetchPredictorRedirectStatusHistogram,
      static_cast<int>(redirect_status), static_cast<int>(RedirectStatus::MAX));
}

void ReportPrefetchAccuracy(const ResourcePrefetcher::PrefetcherStats& stats,
                            const std::vector<URLRequestSummary>& requests) {
  if (stats.requests_stats.empty())
    return;

  std::set<GURL> urls;
  for (const auto& request : requests)
    urls.emplace(request.resource_url);

  int cached_misses_count = 0;
  int not_cached_misses_count = 0;
  int cached_hits_count = 0;
  int not_cached_hits_count = 0;
  int64_t misses_bytes = 0;
  int64_t hits_bytes = 0;

  for (const auto& request_stats : stats.requests_stats) {
    bool hit = urls.find(request_stats.resource_url) != urls.end();
    bool cached = request_stats.was_cached;
    size_t bytes = request_stats.total_received_bytes;

    cached_hits_count += cached && hit;
    cached_misses_count += cached && !hit;
    not_cached_hits_count += !cached && hit;
    not_cached_misses_count += !cached && !hit;
    misses_bytes += !hit * bytes;
    hits_bytes += hit * bytes;
  }

  UMA_HISTOGRAM_COUNTS_100(
      internal::kResourcePrefetchPredictorPrefetchMissesCountCached,
      cached_misses_count);
  UMA_HISTOGRAM_COUNTS_100(
      internal::kResourcePrefetchPredictorPrefetchMissesCountNotCached,
      not_cached_misses_count);
  UMA_HISTOGRAM_COUNTS_100(
      internal::kResourcePrefetchPredictorPrefetchHitsCountCached,
      cached_hits_count);
  UMA_HISTOGRAM_COUNTS_100(
      internal::kResourcePrefetchPredictorPrefetchHitsCountNotCached,
      not_cached_hits_count);
  UMA_HISTOGRAM_COUNTS_10000(
      internal::kResourcePrefetchPredictorPrefetchHitsSize, hits_bytes / 1024);
  UMA_HISTOGRAM_COUNTS_10000(
      internal::kResourcePrefetchPredictorPrefetchMissesSize,
      misses_bytes / 1024);
}

}  // namespace

LoadingStatsCollector::LoadingStatsCollector(
    ResourcePrefetchPredictor* predictor,
    const LoadingPredictorConfig& config)
    : predictor_(predictor),
      max_stats_age_(base::TimeDelta::FromSeconds(
          config.max_navigation_lifetime_seconds)) {}

LoadingStatsCollector::~LoadingStatsCollector() = default;

void LoadingStatsCollector::RecordPrefetcherStats(
    std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats) {
  const GURL main_frame_url = stats->url;
  auto it = prefetcher_stats_.find(main_frame_url);
  if (it != prefetcher_stats_.end()) {
    // No requests -> everything is a miss.
    ReportPrefetchAccuracy(*it->second, std::vector<URLRequestSummary>());
    prefetcher_stats_.erase(it);
  }

  prefetcher_stats_.emplace(main_frame_url, std::move(stats));
}

void LoadingStatsCollector::RecordPageRequestSummary(
    const PageRequestSummary& summary) {
  const GURL& initial_url = summary.initial_url;

  ResourcePrefetchPredictor::Prediction prediction;
  if (predictor_->GetPrefetchData(initial_url, &prediction))
    ReportPredictionAccuracy(prediction, summary);

  auto it = prefetcher_stats_.find(initial_url);
  if (it != prefetcher_stats_.end()) {
    ReportPrefetchAccuracy(*it->second, summary.subresource_requests);
    prefetcher_stats_.erase(it);
  }
}

void LoadingStatsCollector::CleanupAbandonedStats() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  for (auto it = prefetcher_stats_.begin(); it != prefetcher_stats_.end();) {
    if (time_now - it->second->start_time > max_stats_age_) {
      // No requests -> everything is a miss.
      ReportPrefetchAccuracy(*it->second, std::vector<URLRequestSummary>());
      it = prefetcher_stats_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace predictors
