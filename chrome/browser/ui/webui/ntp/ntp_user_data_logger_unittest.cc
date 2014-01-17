// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/common/ntp_logging_events.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestNTPUserDataLogger : public NTPUserDataLogger {
 public:
  TestNTPUserDataLogger() : NTPUserDataLogger(NULL) {}
  virtual ~TestNTPUserDataLogger() {}
};

base::HistogramBase::Count GetTotalCount(const std::string& histogram_name) {
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  return histogram->SnapshotSamples()->TotalCount();
}

base::HistogramBase::Count GetBinCount(const std::string& histogram_name,
                                       base::HistogramBase::Sample value) {
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  return histogram->SnapshotSamples()->GetCount(value);
}

}  // namespace

TEST(NTPUserDataLoggerTest, TestLogging) {
  base::StatisticsRecorder::Initialize();
  TestNTPUserDataLogger logger;

  // Ensure it works when the statistics are all empty. Only the mouseover
  // should be logged in this case. The other histograms are not created yet so
  // we can't query them.
  logger.EmitNtpStatistics();

  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfMouseOvers"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfMouseOvers", 0));

  // Ensure it works with some non-zero statistics. All statistics should now
  // be logged.
  for (int i = 0; i < 20; ++i)
    logger.LogEvent(NTP_MOUSEOVER);
  for (int i = 0; i < 8; ++i)
    logger.LogEvent(NTP_TILE);
  for (int i = 0; i < 4; ++i)
    logger.LogEvent(NTP_THUMBNAIL_TILE);
  for (int i = 0; i < 2; ++i)
    logger.LogEvent(NTP_THUMBNAIL_ERROR);
  logger.LogEvent(NTP_GRAY_TILE_FALLBACK);
  logger.LogEvent(NTP_EXTERNAL_TILE_FALLBACK);
  for (int i = 0; i < 2; ++i)
    logger.LogEvent(NTP_EXTERNAL_TILE);
  for (int i = 0; i < 2; ++i)
    logger.LogEvent(NTP_GRAY_TILE);
  logger.LogEvent(NTP_SERVER_SIDE_SUGGESTION);
  logger.EmitNtpStatistics();

  EXPECT_EQ(2, GetTotalCount("NewTabPage.NumberOfMouseOvers"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfMouseOvers", 0));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfMouseOvers", 20));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfTiles"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfTiles", 8));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfThumbnailTiles"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfThumbnailTiles", 4));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfThumbnailErrors"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfThumbnailErrors", 2));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfGrayTileFallbacks"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfGrayTileFallbacks", 1));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfExternalTileFallbacks"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfExternalTileFallbacks", 1));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfExternalTiles"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfExternalTiles", 2));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfGrayTiles"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfGrayTiles", 2));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.SuggestionsType"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsType", 1));

  // Statistics should be reset to 0, so we should not log anything else.
  logger.EmitNtpStatistics();
  EXPECT_EQ(3, GetTotalCount("NewTabPage.NumberOfMouseOvers"));
  EXPECT_EQ(2, GetBinCount("NewTabPage.NumberOfMouseOvers", 0));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfMouseOvers", 20));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfTiles"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfThumbnailTiles"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfThumbnailErrors"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfGrayTileFallbacks"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfExternalTileFallbacks"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfExternalTiles"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfGrayTiles"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.SuggestionsType"));
}
