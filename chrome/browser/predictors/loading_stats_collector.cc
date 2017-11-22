// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include <set>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"

namespace predictors {

namespace {

using RedirectStatus = ResourcePrefetchPredictor::RedirectStatus;

RedirectStatus GetPredictionRedirectStatus(const GURL& initial_url,
                                           const GURL& main_frame_url,
                                           const std::string& prediction_key,
                                           bool is_redirected,
                                           bool is_host) {
  if (main_frame_url == initial_url) {
    // The actual navigation wasn't redirected.
    return is_redirected ? RedirectStatus::NO_REDIRECT_BUT_PREDICTED
                         : RedirectStatus::NO_REDIRECT;
  }

  if (!is_redirected)
    return RedirectStatus::REDIRECT_NOT_PREDICTED;

  const std::string& main_frame_key =
      is_host ? main_frame_url.host() : main_frame_url.spec();
  return main_frame_key == prediction_key
             ? RedirectStatus::REDIRECT_CORRECTLY_PREDICTED
             : RedirectStatus::REDIRECT_WRONG_PREDICTED;
}

void ReportPredictionAccuracy(
    const ResourcePrefetchPredictor::Prediction& prediction,
    const PageRequestSummary& summary) {
  const std::vector<GURL>& predicted_urls = prediction.subresource_urls;
  if (predicted_urls.empty() || summary.subresource_requests.empty())
    return;

  std::set<GURL> actual_urls_set;
  for (const auto& request_summary : summary.subresource_requests)
    actual_urls_set.emplace(request_summary.resource_url);

  size_t correctly_predicted_count = 0;
  for (const GURL& predicted_url : predicted_urls) {
    if (actual_urls_set.find(predicted_url) != actual_urls_set.end())
      correctly_predicted_count++;
  }

  size_t precision_percentage =
      (100 * correctly_predicted_count) / predicted_urls.size();
  size_t recall_percentage =
      (100 * correctly_predicted_count) / actual_urls_set.size();

  UMA_HISTOGRAM_PERCENTAGE(
      internal::kResourcePrefetchPredictorPrecisionHistogram,
      precision_percentage);
  UMA_HISTOGRAM_PERCENTAGE(internal::kResourcePrefetchPredictorRecallHistogram,
                           recall_percentage);
  UMA_HISTOGRAM_COUNTS_100(internal::kResourcePrefetchPredictorCountHistogram,
                           predicted_urls.size());

  RedirectStatus redirect_status = GetPredictionRedirectStatus(
      summary.initial_url, summary.main_frame_url, prediction.main_frame_key,
      prediction.is_redirected, prediction.is_host);

  UMA_HISTOGRAM_ENUMERATION(
      internal::kResourcePrefetchPredictorRedirectStatusHistogram,
      static_cast<int>(redirect_status), static_cast<int>(RedirectStatus::MAX));
}

void ReportPreconnectPredictionAccuracy(const PreconnectPrediction& prediction,
                                        const PageRequestSummary& summary) {
  if (prediction.requests.empty() || summary.origins.empty())
    return;

  const auto& actual_origins = summary.origins;

  size_t correctly_predicted_count = std::count_if(
      prediction.requests.begin(), prediction.requests.end(),
      [&actual_origins](const PreconnectRequest& request) {
        return actual_origins.find(request.origin) != actual_origins.end();
      });
  size_t precision_percentage =
      (100 * correctly_predicted_count) / prediction.requests.size();
  size_t recall_percentage =
      (100 * correctly_predicted_count) / actual_origins.size();

  UMA_HISTOGRAM_PERCENTAGE(
      internal::kLoadingPredictorPreconnectLearningPrecision,
      precision_percentage);
  UMA_HISTOGRAM_PERCENTAGE(internal::kLoadingPredictorPreconnectLearningRecall,
                           recall_percentage);
  UMA_HISTOGRAM_COUNTS_100(internal::kLoadingPredictorPreconnectLearningCount,
                           prediction.requests.size());

  RedirectStatus redirect_status = GetPredictionRedirectStatus(
      summary.initial_url, summary.main_frame_url, prediction.host,
      prediction.is_redirected, true /* is_host */);

  UMA_HISTOGRAM_ENUMERATION(
      internal::kLoadingPredictorPreconnectLearningRedirectStatus,
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

void ReportPreconnectAccuracy(
    const PreconnectStats& stats,
    const std::map<GURL, OriginRequestSummary>& requests) {
  if (stats.requests_stats.empty())
    return;

  int preresolve_hits_count = 0;
  int preresolve_misses_count = 0;
  int preconnect_hits_count = 0;
  int preconnect_misses_count = 0;

  for (const auto& request_stats : stats.requests_stats) {
    bool hit = requests.find(request_stats.origin) != requests.end();
    bool preconnect = request_stats.was_preconnected;

    preresolve_hits_count += hit;
    preresolve_misses_count += !hit;
    preconnect_hits_count += preconnect && hit;
    preconnect_misses_count += preconnect && !hit;
  }

  int total_preresolves = preresolve_hits_count + preresolve_misses_count;
  int total_preconnects = preconnect_hits_count + preconnect_misses_count;
  DCHECK_EQ(static_cast<int>(stats.requests_stats.size()),
            preresolve_hits_count + preresolve_misses_count);
  DCHECK_GT(total_preresolves, 0);

  size_t preresolve_hits_percentage =
      (100 * preresolve_hits_count) / total_preresolves;

  if (total_preconnects > 0) {
    size_t preconnect_hits_percentage =
        (100 * preconnect_hits_count) / total_preconnects;
    UMA_HISTOGRAM_PERCENTAGE(
        internal::kLoadingPredictorPreconnectHitsPercentage,
        preconnect_hits_percentage);
  }

  UMA_HISTOGRAM_PERCENTAGE(internal::kLoadingPredictorPreresolveHitsPercentage,
                           preresolve_hits_percentage);
  UMA_HISTOGRAM_COUNTS_100(internal::kLoadingPredictorPreresolveCount,
                           total_preresolves);
  UMA_HISTOGRAM_COUNTS_100(internal::kLoadingPredictorPreconnectCount,
                           total_preconnects);
}

void ReportPageLoadStats(const PageRequestSummary& summary) {
  for (const auto& subresource : summary.subresource_requests) {
    if (!subresource.network_accessed ||
        !subresource.before_first_contentful_paint) {
      continue;
    }

    UMA_HISTOGRAM_TIMES(internal::kLoadingPredictorSubresourceConnectDuration,
                        subresource.connect_duration);
  }
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
  const GURL& main_frame_url = stats->url;
  auto it = prefetcher_stats_.find(main_frame_url);
  if (it != prefetcher_stats_.end()) {
    // No requests -> everything is a miss.
    ReportPrefetchAccuracy(*it->second, std::vector<URLRequestSummary>());
    prefetcher_stats_.erase(it);
  }

  prefetcher_stats_.emplace(main_frame_url, std::move(stats));
}

void LoadingStatsCollector::RecordPreconnectStats(
    std::unique_ptr<PreconnectStats> stats) {
  const GURL& main_frame_url = stats->url;
  auto it = preconnect_stats_.find(main_frame_url);
  if (it != preconnect_stats_.end()) {
    ReportPreconnectAccuracy(*it->second,
                             std::map<GURL, OriginRequestSummary>());
    preconnect_stats_.erase(it);
  }

  preconnect_stats_.emplace(main_frame_url, std::move(stats));
}

void LoadingStatsCollector::RecordPageRequestSummary(
    const PageRequestSummary& summary) {
  ReportPageLoadStats(summary);

  const GURL& initial_url = summary.initial_url;

  ResourcePrefetchPredictor::Prediction prediction;
  if (predictor_->GetPrefetchData(initial_url, &prediction))
    ReportPredictionAccuracy(prediction, summary);

  PreconnectPrediction preconnect_prediction;
  if (predictor_->PredictPreconnectOrigins(initial_url, &preconnect_prediction))
    ReportPreconnectPredictionAccuracy(preconnect_prediction, summary);

  auto it = prefetcher_stats_.find(initial_url);
  if (it != prefetcher_stats_.end()) {
    ReportPrefetchAccuracy(*it->second, summary.subresource_requests);
    prefetcher_stats_.erase(it);
  }

  auto it2 = preconnect_stats_.find(initial_url);
  if (it2 != preconnect_stats_.end()) {
    ReportPreconnectAccuracy(*it2->second, summary.origins);
    preconnect_stats_.erase(it2);
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
  for (auto it = preconnect_stats_.begin(); it != preconnect_stats_.end();) {
    if (time_now - it->second->start_time > max_stats_age_) {
      ReportPreconnectAccuracy(*it->second,
                               std::map<GURL, OriginRequestSummary>());
      it = preconnect_stats_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace predictors
