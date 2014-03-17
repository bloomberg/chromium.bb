// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
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
  // Return 0 if history is uninitialized.
  return histogram ? histogram->SnapshotSamples()->TotalCount() : 0;
}

base::HistogramBase::Count GetBinCount(const std::string& histogram_name,
                                       base::HistogramBase::Sample value) {
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  // Return 0 if history is uninitialized.
  return histogram ? histogram->SnapshotSamples()->GetCount(value) : 0;
}

}  // namespace

TEST(NTPUserDataLoggerTest, TestLogging) {
  base::StatisticsRecorder::Initialize();

  // Ensure empty statistics.
  EXPECT_EQ(0, GetTotalCount("NewTabPage.NumberOfMouseOvers"));
  EXPECT_EQ(0, GetBinCount("NewTabPage.NumberOfMouseOvers", 0));

  // Enusure non-zero statistics.
  TestNTPUserDataLogger logger;

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

  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfMouseOvers"));
  EXPECT_EQ(0, GetBinCount("NewTabPage.NumberOfMouseOvers", 0));
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
  EXPECT_EQ(2, GetTotalCount("NewTabPage.NumberOfMouseOvers"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfMouseOvers", 0));
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

TEST(NTPUserDataLoggerTest, TestLogMostVisitedImpression) {
  base::StatisticsRecorder::Initialize();

  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 5));

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedImpression(1, base::ASCIIToUTF16("foobar"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 5));

  logger.LogMostVisitedImpression(5, base::ASCIIToUTF16("foobar"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 5));

  // Try without provider. Only total increases.
  logger.LogMostVisitedImpression(5, base::ASCIIToUTF16(""));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 5));

  logger.LogMostVisitedImpression(1, base::ASCIIToUTF16("foobar"));
  EXPECT_EQ(2, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.foobar", 5));
}

TEST(NTPUserDataLoggerTest, TestLogMostVisitedNavigation) {
  base::StatisticsRecorder::Initialize();

  EXPECT_EQ(0, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.foobar", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.foobar", 5));

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedNavigation(1, base::ASCIIToUTF16("foobar"));
  EXPECT_EQ(1, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.foobar", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.foobar", 5));

  logger.LogMostVisitedNavigation(5, base::ASCIIToUTF16("foobar"));
  EXPECT_EQ(2, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.foobar", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.foobar", 5));

  // Try without provider. Only total increases.
  logger.LogMostVisitedNavigation(5, base::ASCIIToUTF16(""));
  EXPECT_EQ(3, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.foobar", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.foobar", 5));

  logger.LogMostVisitedNavigation(1, base::ASCIIToUTF16("foobar"));
  EXPECT_EQ(4, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(2, GetBinCount("NewTabPage.MostVisited.foobar", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.foobar", 5));
}
