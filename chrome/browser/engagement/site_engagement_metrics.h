// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_METRICS_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_METRICS_H_

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "url/gurl.h"

// Helper class managing the UMA histograms for the Site Engagement Service.
class SiteEngagementMetrics {
 public:
  // This is used to back a UMA histogram, so it should be treated as
  // append-only. Any new values should be inserted immediately prior to
  // ENGAGEMENT_LAST.
  enum EngagementType {
    ENGAGEMENT_NAVIGATION,
    ENGAGEMENT_KEYPRESS,
    ENGAGEMENT_MOUSE,
    ENGAGEMENT_TOUCH_GESTURE,
    ENGAGEMENT_SCROLL,
    ENGAGEMENT_MEDIA_HIDDEN,
    ENGAGEMENT_MEDIA_VISIBLE,
    ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH,
    ENGAGEMENT_FIRST_DAILY_ENGAGEMENT,
    ENGAGEMENT_LAST,
  };

  static void RecordTotalSiteEngagement(double total_engagement);
  static void RecordTotalOriginsEngaged(int total_origins);
  static void RecordMeanEngagement(double mean_engagement);
  static void RecordMedianEngagement(double median_engagement);
  static void RecordEngagementPercentageForHTTPS(double percentage);
  static void RecordEngagementScores(const std::map<GURL, double>& score_map);
  static void RecordOriginsWithMaxEngagement(int total_origins);
  static void RecordOriginsWithMaxDailyEngagement(int total_origins);
  static void RecordPercentOriginsWithMaxEngagement(double percentage);
  static void RecordEngagement(EngagementType type);
  static void RecordDaysSinceLastShortcutLaunch(int days);
  static void RecordScoreDecayedFrom(double score);
  static void RecordScoreDecayedTo(double score);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CheckHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, LastShortcutLaunch);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, ScoreDecayHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementHelperTest,
                           MixedInputEngagementAccumulation);
  static const char kTotalEngagementHistogram[];
  static const char kTotalOriginsHistogram[];
  static const char kMeanEngagementHistogram[];
  static const char kMedianEngagementHistogram[];
  static const char kEngagementPercentageForHTTPSHistogram[];
  static const char kEngagementScoreHistogram[];
  static const char kEngagementScoreHistogramHTTP[];
  static const char kEngagementScoreHistogramHTTPS[];
  static const char kEngagementScoreHistogramIsZero[];
  static const char kOriginsWithMaxEngagementHistogram[];
  static const char kOriginsWithMaxDailyEngagementHistogram[];
  static const char kPercentOriginsWithMaxEngagementHistogram[];
  static const char kEngagementTypeHistogram[];
  static const char kEngagementBucketHistogramBase[];
  static const char kDaysSinceLastShortcutLaunchHistogram[];
  static const char kScoreDecayedFromHistogram[];
  static const char kScoreDecayedToHistogram[];

  static std::vector<std::string> GetEngagementBucketHistogramNames();
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_METRICS_H_
