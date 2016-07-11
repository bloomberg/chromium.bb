// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
                                       base::HistogramBase::Sample value) {
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  // Return 0 if history is uninitialized.
  return histogram ? histogram->SnapshotSamples()->GetCount(value) : 0;
}

}  // namespace

class NTPUserDataLoggerTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(NTPUserDataLoggerTest, TestLogging) {
  base::StatisticsRecorder::Initialize();

  // Enusure non-zero statistics.
  TestNTPUserDataLogger logger;

  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(0);

  for (int i = 0; i < 8; ++i)
    logger.LogEvent(NTP_TILE, delta);
  logger.LogEvent(NTP_SERVER_SIDE_SUGGESTION, delta);

  logger.EmitNtpStatistics(NTPUserDataLogger::EmitReason::NAVIGATED_AWAY);

  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfTiles"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.NumberOfTiles", 8));

  // Statistics should be reset to 0, so we should not log anything else.
  logger.EmitNtpStatistics(NTPUserDataLogger::EmitReason::NAVIGATED_AWAY);
  EXPECT_EQ(1, GetTotalCount("NewTabPage.NumberOfTiles"));
}

TEST_F(NTPUserDataLoggerTest, TestLogMostVisitedImpression) {
  base::StatisticsRecorder::Initialize();

  EXPECT_EQ(0, GetTotalCount("NewTabPage.SuggestionsImpression"));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.server", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.server", 5));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 5));

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::SERVER);
  EXPECT_EQ(1, GetTotalCount("NewTabPage.SuggestionsImpression"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.server", 5));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 5));

  logger.LogMostVisitedImpression(5, NTPLoggingTileSource::SERVER);
  EXPECT_EQ(2, GetTotalCount("NewTabPage.SuggestionsImpression"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 5));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 5));

  logger.LogMostVisitedImpression(1, NTPLoggingTileSource::CLIENT);
  EXPECT_EQ(3, GetTotalCount("NewTabPage.SuggestionsImpression"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 5));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.SuggestionsImpression.client", 5));

  logger.LogMostVisitedImpression(5, NTPLoggingTileSource::CLIENT);
  EXPECT_EQ(4, GetTotalCount("NewTabPage.SuggestionsImpression"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.server", 5));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.client", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.SuggestionsImpression.client", 5));
}

TEST_F(NTPUserDataLoggerTest, TestLogMostVisitedNavigation) {
  base::StatisticsRecorder::Initialize();

  EXPECT_EQ(0, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.server", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.server", 5));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 5));

  TestNTPUserDataLogger logger;

  logger.LogMostVisitedNavigation(1, NTPLoggingTileSource::SERVER);
  EXPECT_EQ(1, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.server", 5));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 5));

  logger.LogMostVisitedNavigation(5, NTPLoggingTileSource::SERVER);
  EXPECT_EQ(2, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 5));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 5));

  logger.LogMostVisitedNavigation(1, NTPLoggingTileSource::CLIENT);
  EXPECT_EQ(3, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 5));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.client", 1));
  EXPECT_EQ(0, GetBinCount("NewTabPage.MostVisited.client", 5));

  logger.LogMostVisitedNavigation(5, NTPLoggingTileSource::CLIENT);
  EXPECT_EQ(4, GetTotalCount("NewTabPage.MostVisited"));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.server", 5));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.client", 1));
  EXPECT_EQ(1, GetBinCount("NewTabPage.MostVisited.client", 5));
}
