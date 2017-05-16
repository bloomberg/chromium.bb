// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/ntp_user_data_logger.h"

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
using ntp_tiles::TileSource;
using ntp_tiles::TileVisualType;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

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

  for (int i = 0; i < 8; ++i) {
    logger.LogMostVisitedImpression(i, TileSource::SUGGESTIONS_SERVICE,
                                    TileVisualType::THUMBNAIL);
  }
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
  logger.LogMostVisitedImpression(0, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(1, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL_FAILED);
  logger.LogMostVisitedImpression(2, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(3, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(4, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(5, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(6, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(7, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);

  // Repeated impressions for the same bins are ignored.
  logger.LogMostVisitedImpression(0, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL_FAILED);
  logger.LogMostVisitedImpression(1, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL_FAILED);
  logger.LogMostVisitedImpression(2, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(3, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);

  // Impressions are silently ignored for tiles >= 8.
  logger.LogMostVisitedImpression(8, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(9, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);

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
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"), IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.server"),
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
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 7),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 3),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 4)));

  // After navigating away from the NTP and back, we record again.
  logger.NavigatedFromURLToURL(GURL("chrome://newtab/"),
                               GURL("http://chromium.org"));
  logger.NavigatedFromURLToURL(GURL("http://chromium.org"),
                               GURL("chrome://newtab/"));
  logger.LogMostVisitedImpression(0, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(1, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(2, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedImpression(3, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL_FAILED);
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
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 10),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 5),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 5),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
}

TEST(NTPUserDataLoggerTest, TestLogMostVisitedNavigation) {
  base::StatisticsRecorder::Initialize();

  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedNavigation(0, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              ElementsAre(Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
              ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
      IsEmpty());

  logger.LogMostVisitedNavigation(1, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL_FAILED);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
      IsEmpty());

  logger.LogMostVisitedNavigation(2, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 2),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1)));

  logger.LogMostVisitedNavigation(3, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL_FAILED);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 2),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 1),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));

  // Navigations always increase.
  logger.LogMostVisitedNavigation(0, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedNavigation(1, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedNavigation(2, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);
  logger.LogMostVisitedNavigation(3, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
      ElementsAre(Bucket(0, 2), Bucket(1, 2), Bucket(2, 2), Bucket(3, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              ElementsAre(Bucket(0, 2), Bucket(1, 1), Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(Bucket(1, 1), Bucket(2, 1), Bucket(3, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 6),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.server"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 3),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTypeClicked.client"),
      ElementsAre(Bucket(ntp_tiles::TileVisualType::THUMBNAIL, 3),
                  Bucket(ntp_tiles::TileVisualType::THUMBNAIL_FAILED, 1)));
}

TEST(NTPUserDataLoggerTest, TestLoadTime) {
  base::StatisticsRecorder::Initialize();

  base::HistogramTester histogram_tester;

  TestNTPUserDataLogger logger;
  logger.ntp_url_ = GURL("chrome://newtab/");

  base::TimeDelta delta_tiles_received = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delta_tiles_loaded = base::TimeDelta::FromMilliseconds(100);

  // Send the ALL_TILES_RECEIVED event.
  logger.LogEvent(NTP_ALL_TILES_RECEIVED, delta_tiles_received);

  // Log a TOP_SITES impression (for the .MostVisited vs .MostLikely split in
  // the time histograms).
  logger.LogMostVisitedImpression(0, TileSource::TOP_SITES,
                                  TileVisualType::THUMBNAIL);

  // Send the ALL_TILES_LOADED event, this should trigger emitting histograms.
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta_tiles_loaded);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TilesReceivedTime"),
              SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.TilesReceivedTime.MostVisited"),
              SizeIs(1));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TilesReceivedTime.MostLikely"),
      IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime"), SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.MostVisited"),
              SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.MostLikely"),
              IsEmpty());

  histogram_tester.ExpectTimeBucketCount("NewTabPage.TilesReceivedTime",
                                         delta_tiles_received, 1);
  histogram_tester.ExpectTimeBucketCount(
      "NewTabPage.TilesReceivedTime.MostVisited", delta_tiles_received, 1);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime",
                                         delta_tiles_loaded, 1);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime.MostVisited",
                                         delta_tiles_loaded, 1);

  // We should not log again for the same NTP.
  logger.LogEvent(NTP_ALL_TILES_RECEIVED, delta_tiles_received);
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta_tiles_loaded);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.TilesReceivedTime",
                                         delta_tiles_received, 1);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime",
                                         delta_tiles_loaded, 1);

  // After navigating away from the NTP and back, we record again.
  logger.NavigatedFromURLToURL(GURL("chrome://newtab/"),
                               GURL("http://chromium.org"));
  logger.NavigatedFromURLToURL(GURL("http://chromium.org"),
                               GURL("chrome://newtab/"));

  // This time, log a SUGGESTIONS_SERVICE impression, so the times will end up
  // in .MostLikely.
  logger.LogMostVisitedImpression(0, TileSource::SUGGESTIONS_SERVICE,
                                  TileVisualType::THUMBNAIL);

  base::TimeDelta delta_tiles_received2 = base::TimeDelta::FromMilliseconds(50);
  base::TimeDelta delta_tiles_loaded2 = base::TimeDelta::FromMilliseconds(500);
  logger.LogEvent(NTP_ALL_TILES_RECEIVED, delta_tiles_received2);
  logger.LogEvent(NTP_ALL_TILES_LOADED, delta_tiles_loaded2);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TilesReceivedTime"),
              SizeIs(2));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.TilesReceivedTime.MostVisited"),
              SizeIs(1));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TilesReceivedTime.MostLikely"),
      SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime"), SizeIs(2));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.MostVisited"),
              SizeIs(1));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.LoadTime.MostLikely"),
              SizeIs(1));

  histogram_tester.ExpectTimeBucketCount("NewTabPage.TilesReceivedTime",
                                         delta_tiles_received2, 1);
  histogram_tester.ExpectTimeBucketCount(
      "NewTabPage.TilesReceivedTime.MostLikely", delta_tiles_received2, 1);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime",
                                         delta_tiles_loaded2, 1);
  histogram_tester.ExpectTimeBucketCount("NewTabPage.LoadTime.MostLikely",
                                         delta_tiles_loaded2, 1);
}
