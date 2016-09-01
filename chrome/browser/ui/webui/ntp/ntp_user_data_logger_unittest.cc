// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using Sample = base::HistogramBase::Sample;
using Samples = std::vector<Sample>;

class TestNTPUserDataLogger : public NTPUserDataLogger {
 public:
  TestNTPUserDataLogger() : NTPUserDataLogger(NULL) {}
  ~TestNTPUserDataLogger() override {}
};

base::HistogramBase::Count GetTotalCount(const std::string& histogram_name) {
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  // Return 0 if history is uninitialized.
  return histogram ? histogram->SnapshotSamples()->TotalCount() : 0;
}

base::HistogramBase::Count GetBinCount(const std::string& histogram_name,
                                       Sample value) {
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  // Return 0 if history is uninitialized.
  return histogram ? histogram->SnapshotSamples()->GetCount(value) : 0;
}

std::vector<int> GetBinCounts(const std::string& histogram_name,
                              const Samples& values) {
  std::vector<int> results;
  for (const auto& value : values) {
    results.push_back(GetBinCount(histogram_name, value));
  }
  return results;
}

std::vector<int> TotalImpressions(const Samples& values) {
  return GetBinCounts("NewTabPage.SuggestionsImpression", values);
}

std::vector<int> ServerImpressions(const Samples& values) {
  return GetBinCounts("NewTabPage.SuggestionsImpression.server", values);
}

std::vector<int> ClientImpressions(const Samples& values) {
  return GetBinCounts("NewTabPage.SuggestionsImpression.client", values);
}

std::vector<int> TotalNavigations(const Samples& values) {
  return GetBinCounts("NewTabPage.MostVisited", values);
}

std::vector<int> ServerNavigations(const Samples& values) {
  return GetBinCounts("NewTabPage.MostVisited.server", values);
}

std::vector<int> ClientNavigations(const Samples& values) {
  return GetBinCounts("NewTabPage.MostVisited.client", values);
}

}  // namespace

class NTPUserDataLoggerTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(NTPUserDataLoggerTest, TestNumberOfTiles) {
  base::StatisticsRecorder::Initialize();

  // Enusure non-zero statistics.
  TestNTPUserDataLogger logger;
  logger.ntp_url_ = GURL("chrome://newtab/");

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(0);

  for (int i = 0; i < 8; ++i)
    logger.LogEvent(NTP_TILE, delta);
  logger.LogEvent(NTP_SERVER_SIDE_SUGGESTION, delta);

  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);

  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfTiles"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfTiles", 8));

  // Statistics should be reset to 0, so we should not log anything else.
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfTiles"));

  // Navigating away and back resets stats.
  logger.NavigatedFromURLToURL(GURL("chrome://newtab/"),
                               GURL("http://chromium.org"));
  logger.NavigatedFromURLToURL(GURL("http://chromium.org"),
                               GURL("chrome://newtab/"));
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);
  EXPECT_EQ(2, GetTotalCount("NewTabPage.NumberOfTiles"));
}

TEST_F(NTPUserDataLoggerTest, TestLogMostVisitedImpression) {
  base::StatisticsRecorder::Initialize();
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));

  TestNTPUserDataLogger logger;
  logger.ntp_url_ = GURL("chrome://newtab/");

  // Impressions increment the associated bins.

  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::SERVER);
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(1, 0, 0, 0));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(1, 0, 0, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));

  logger.LogMostVisitedImpression(2, NTPLoggingTileSource::SERVER);
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));

  logger.LogMostVisitedImpression(3, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 1, 0));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 1, 0));

  logger.LogMostVisitedImpression(4, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 1, 1));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 1, 1));

  // But once incremented, they don't increase again unless reset.
  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(2, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(3, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(4, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 1, 1));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 0, 1, 1));

  // Impressions are silently ignored for tiles >= 8.
  logger.LogMostVisitedImpression(8, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(9, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalImpressions({8, 9}), ElementsAre(0, 0));
  EXPECT_THAT(ServerImpressions({8, 9}), ElementsAre(0, 0));
  EXPECT_THAT(ClientImpressions({8, 9}), ElementsAre(0, 0));

  // Navigating away from the NTP and back resets stats.
  logger.NavigatedFromURLToURL(GURL("chrome://newtab/"),
                               GURL("http://chromium.org"));
  logger.NavigatedFromURLToURL(GURL("http://chromium.org"),
                               GURL("chrome://newtab/"));
  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(2, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(3, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(4, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalImpressions({1, 2, 3, 4}), ElementsAre(2, 2, 2, 2));
  EXPECT_THAT(ServerImpressions({1, 2, 3, 4}), ElementsAre(2, 1, 1, 0));
  EXPECT_THAT(ClientImpressions({1, 2, 3, 4}), ElementsAre(0, 1, 1, 2));
}

TEST_F(NTPUserDataLoggerTest, TestLogMostVisitedNavigation) {
  base::StatisticsRecorder::Initialize();

  EXPECT_THAT(TotalNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(ServerNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(ClientNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedNavigation(1, NTPLoggingTileSource::SERVER);
  EXPECT_THAT(TotalNavigations({1, 2, 3, 4}), ElementsAre(1, 0, 0, 0));
  EXPECT_THAT(ServerNavigations({1, 2, 3, 4}), ElementsAre(1, 0, 0, 0));
  EXPECT_THAT(ClientNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));

  logger.LogMostVisitedNavigation(2, NTPLoggingTileSource::SERVER);
  EXPECT_THAT(TotalNavigations({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ServerNavigations({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 0, 0));

  logger.LogMostVisitedNavigation(3, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalNavigations({1, 2, 3, 4}), ElementsAre(1, 1, 1, 0));
  EXPECT_THAT(ServerNavigations({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 1, 0));

  logger.LogMostVisitedNavigation(4, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalNavigations({1, 2, 3, 4}), ElementsAre(1, 1, 1, 1));
  EXPECT_THAT(ServerNavigations({1, 2, 3, 4}), ElementsAre(1, 1, 0, 0));
  EXPECT_THAT(ClientNavigations({1, 2, 3, 4}), ElementsAre(0, 0, 1, 1));

  // Navigations always increase.
  logger.LogMostVisitedNavigation(1, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedNavigation(2, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedNavigation(3, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedNavigation(4, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(TotalNavigations({1, 2, 3, 4}), ElementsAre(2, 2, 2, 2));
  EXPECT_THAT(ServerNavigations({1, 2, 3, 4}), ElementsAre(2, 1, 1, 0));
  EXPECT_THAT(ClientNavigations({1, 2, 3, 4}), ElementsAre(0, 1, 1, 2));
}
