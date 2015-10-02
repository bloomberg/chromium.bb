// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"

const char SiteEngagementMetrics::kTotalEngagementHistogram[] =
    "SiteEngagementService.TotalEngagement";

const char SiteEngagementMetrics::kTotalOriginsHistogram[] =
    "SiteEngagementService.OriginsEngaged";

const char SiteEngagementMetrics::kMeanEngagementHistogram[] =
    "SiteEngagementService.MeanEngagement";

const char SiteEngagementMetrics::kMedianEngagementHistogram[] =
    "SiteEngagementService.MedianEngagement";

const char SiteEngagementMetrics::kEngagementScoreHistogram[] =
    "SiteEngagementService.EngagementScore";

const char SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram[] =
    "SiteEngagementService.OriginsWithMaxEngagement";

const char SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram[] =
    "SiteEngagementService.OriginsWithMaxDailyEngagement";

const char SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram[] =
    "SiteEngagementService.PercentOriginsWithMaxEngagement";

const char SiteEngagementMetrics::kEngagementTypeHistogram[] =
    "SiteEngagementService.EngagementType";

void SiteEngagementMetrics::RecordTotalSiteEngagement(
    double total_engagement) {
  UMA_HISTOGRAM_COUNTS_10000(kTotalEngagementHistogram, total_engagement);
}

void SiteEngagementMetrics::RecordTotalOriginsEngaged(int num_origins) {
  UMA_HISTOGRAM_COUNTS_10000(kTotalOriginsHistogram, num_origins);
}

void SiteEngagementMetrics::RecordMeanEngagement(double mean_engagement) {
  UMA_HISTOGRAM_COUNTS_100(kMeanEngagementHistogram, mean_engagement);
}

void SiteEngagementMetrics::RecordMedianEngagement(double median_engagement) {
  UMA_HISTOGRAM_COUNTS_100(kMedianEngagementHistogram, median_engagement);
}

void SiteEngagementMetrics::RecordEngagementScores(
    std::map<GURL, double> score_map) {
  for (const auto& value: score_map) {
    UMA_HISTOGRAM_COUNTS_100(kEngagementScoreHistogram, value.second);
  }
}

void SiteEngagementMetrics::RecordOriginsWithMaxEngagement(int total_origins) {
  UMA_HISTOGRAM_COUNTS_100(kOriginsWithMaxEngagementHistogram, total_origins);
}

void SiteEngagementMetrics::RecordOriginsWithMaxDailyEngagement(
    int total_origins) {
  UMA_HISTOGRAM_COUNTS_100(kOriginsWithMaxDailyEngagementHistogram,
                           total_origins);
}

void SiteEngagementMetrics::RecordPercentOriginsWithMaxEngagement(
    double percentage) {
  UMA_HISTOGRAM_COUNTS_100(kPercentOriginsWithMaxEngagementHistogram,
                           percentage);
}

void SiteEngagementMetrics::RecordEngagement(EngagementType type) {
  UMA_HISTOGRAM_ENUMERATION(kEngagementTypeHistogram, type, ENGAGEMENT_LAST);
}
