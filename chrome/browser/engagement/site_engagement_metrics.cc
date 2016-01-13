// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"

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

const char SiteEngagementMetrics::kEngagementBucketHistogramBase[] =
    "SiteEngagementService.EngagementScoreBucket_";

const char SiteEngagementMetrics::kDaysSinceLastShortcutLaunchHistogram[] =
    "SiteEngagementService.DaysSinceLastShortcutLaunch";

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
  // Record the percentage of sites that fall in each 10-point wide range. These
  // numbers are used as suffixes for the
  // SiteEngagementService.EngagementScoreBucket_* histogram. If these bases
  // change, the EngagementScoreBuckets suffix in histograms.xml should be
  // updated.
  static const int kBucketBases[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
  std::map<int, int> score_buckets;
  for (size_t i = 0; i < arraysize(kBucketBases); ++i)
    score_buckets[kBucketBases[i]] = 0;

  for (const auto& value: score_map) {
    UMA_HISTOGRAM_COUNTS_100(kEngagementScoreHistogram, value.second);
    score_buckets.lower_bound(value.second)->second++;
  }

  for (const auto& b : score_buckets) {
    std::string histogram_name =
        kEngagementBucketHistogramBase + base::IntToString(b.first);

    base::LinearHistogram::FactoryGet(
        histogram_name, 1, 100, 10,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(b.second * 100 / score_map.size());
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

void SiteEngagementMetrics::RecordDaysSinceLastShortcutLaunch(int days) {
  UMA_HISTOGRAM_COUNTS_100(kDaysSinceLastShortcutLaunchHistogram, days);
}
