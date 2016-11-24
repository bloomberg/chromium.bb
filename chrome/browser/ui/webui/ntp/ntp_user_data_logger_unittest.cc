// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/histogram_tester.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;
using testing::IsEmpty;

namespace {

using Sample = base::HistogramBase::Sample;
using Samples = std::vector<Sample>;

class TestNTPUserDataLogger : public NTPUserDataLogger {
 public:
  TestNTPUserDataLogger() : NTPUserDataLogger(nullptr) {}
  ~TestNTPUserDataLogger() override {}
};

}  // namespace

TEST(NTPUserDataLoggerTest, TestNumberOfTiles) {
  base::StatisticsRecorder::Initialize();

  base::HistogramTester histogram_tester;

  // Ensure non-zero statistics.
  TestNTPUserDataLogger logger;
  logger.ntp_url_ = GURL("chrome://newtab/");

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(0);

  for (int i = 0; i < 8; ++i)
    logger.LogMostVisitedImpression(i, NTPLoggingTileSource::SERVER);
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(Bucket(8, 1)));

  // We should not log again for the same NTP.
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(Bucket(8, 1)));

  // Navigating away and back resets stats.
  logger.NavigatedFromURLToURL(GURL("chrome://newtab/"),
                               GURL("http://chromium.org"));
  logger.NavigatedFromURLToURL(GURL("http://chromium.org"),
                               GURL("chrome://newtab/"));
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(Bucket(0, 1), Bucket(8, 1)));
}

TEST(NTPUserDataLoggerTest, TestLogMostVisitedImpression) {
  base::StatisticsRecorder::Initialize();

  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger;
  logger.ntp_url_ = GURL("chrome://newtab/");

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(0);

  // Impressions increment the associated bins.
  logger.LogMostVisitedImpression(0, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(2, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(3, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(4, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(5, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(6, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(7, NTPLoggingTileSource::CLIENT);

  // Repeated impressions for the same bins are ignored.
  logger.LogMostVisitedImpression(0, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(2, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(3, NTPLoggingTileSource::CLIENT);

  // Impressions are silently ignored for tiles >= 8.
  logger.LogMostVisitedImpression(8, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(9, NTPLoggingTileSource::CLIENT);

  // The actual histograms are emitted only after the ALL_TILES_LOADED event, so
  // at this point everything should still be empty.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.server"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      IsEmpty());

  // Send the ALL_TILES_LOADED event, this should trigger emitting histograms.
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1),
                  Bucket(4, 1), Bucket(5, 1), Bucket(6, 1), Bucket(7, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.server"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ElementsAre(Bucket(4, 1), Bucket(5, 1), Bucket(6, 1), Bucket(7, 1)));

  // After navigating away from the NTP and back, we record again.
  logger.NavigatedFromURLToURL(GURL("chrome://newtab/"),
                               GURL("http://chromium.org"));
  logger.NavigatedFromURLToURL(GURL("http://chromium.org"),
                               GURL("chrome://newtab/"));
  logger.LogMostVisitedImpression(0, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedImpression(2, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedImpression(3, NTPLoggingTileSource::CLIENT);
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ElementsAre(Bucket(0, 2), Bucket(1, 2), Bucket(2, 2), Bucket(3, 2),
                  Bucket(4, 1), Bucket(5, 1), Bucket(6, 1), Bucket(7, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.server"),
      ElementsAre(Bucket(0, 2), Bucket(1, 1), Bucket(2, 2), Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ElementsAre(Bucket(1, 1), Bucket(3, 1), Bucket(4, 1), Bucket(5, 1),
                  Bucket(6, 1), Bucket(7, 1)));
}

TEST(NTPUserDataLoggerTest, TestLogMostVisitedNavigation) {
  base::StatisticsRecorder::Initialize();

  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedNavigation(0, NTPLoggingTileSource::SERVER);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
      ElementsAre(Bucket(0, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
      IsEmpty());

  logger.LogMostVisitedNavigation(1, NTPLoggingTileSource::SERVER);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
      IsEmpty());

  logger.LogMostVisitedNavigation(2, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
      ElementsAre(Bucket(2, 1)));

  logger.LogMostVisitedNavigation(3, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
      ElementsAre(Bucket(2, 1), Bucket(3, 1)));

  // Navigations always increase.
  logger.LogMostVisitedNavigation(0, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedNavigation(1, NTPLoggingTileSource::CLIENT);
  logger.LogMostVisitedNavigation(2, NTPLoggingTileSource::SERVER);
  logger.LogMostVisitedNavigation(3, NTPLoggingTileSource::CLIENT);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 2), Bucket(1, 2), Bucket(2, 2), Bucket(3, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
      ElementsAre(Bucket(0, 2), Bucket(1, 1), Bucket(2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
      ElementsAre(Bucket(1, 1), Bucket(2, 1), Bucket(3, 2)));
}
