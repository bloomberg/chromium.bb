// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/metrics.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "components/rappor/test_rappor_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_tiles {
namespace metrics {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

TEST(RecordPageImpressionTest, ShouldRecordUmaForIcons) {
  base::HistogramTester histogram_tester;
  RecordPageImpression({{TileSource::TOP_SITES, ICON_REAL, GURL()},
                        {TileSource::TOP_SITES, ICON_REAL, GURL()},
                        {TileSource::TOP_SITES, ICON_REAL, GURL()},
                        {TileSource::TOP_SITES, ICON_COLOR, GURL()},
                        {TileSource::TOP_SITES, ICON_COLOR, GURL()},
                        {TileSource::SUGGESTIONS_SERVICE, ICON_REAL, GURL()},
                        {TileSource::SUGGESTIONS_SERVICE, ICON_DEFAULT, GURL()},
                        {TileSource::POPULAR, ICON_COLOR, GURL()}},
                       /*rappor_service=*/nullptr);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(base::Bucket(/*min=*/8, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/2, /*count=*/1),
                  base::Bucket(/*min=*/3, /*count=*/1),
                  base::Bucket(/*min=*/4, /*count=*/1),
                  base::Bucket(/*min=*/5, /*count=*/1),
                  base::Bucket(/*min=*/6, /*count=*/1),
                  base::Bucket(/*min=*/7, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.server"),
      ElementsAre(base::Bucket(/*min=*/5, /*count=*/1),
                  base::Bucket(/*min=*/6, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/2, /*count=*/1),
                  base::Bucket(/*min=*/3, /*count=*/1),
                  base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.popular"),
              ElementsAre(base::Bucket(/*min=*/7, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"),
              ElementsAre(base::Bucket(/*min=*/ICON_REAL, /*count=*/4),
                          base::Bucket(/*min=*/ICON_COLOR, /*count=*/3),
                          base::Bucket(/*min=*/ICON_DEFAULT, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.server"),
              ElementsAre(base::Bucket(/*min=*/ICON_REAL, /*count=*/1),
                          base::Bucket(/*min=*/ICON_DEFAULT, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(base::Bucket(/*min=*/ICON_REAL, /*count=*/3),
                          base::Bucket(/*min=*/ICON_COLOR, /*count=*/2)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.popular"),
              ElementsAre(base::Bucket(/*min=*/ICON_COLOR,
                                       /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.IconsReal"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1),
                          base::Bucket(/*min=*/2, /*count=*/1),
                          base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.IconsColor"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1),
                          base::Bucket(/*min=*/4, /*count=*/1),
                          base::Bucket(/*min=*/7, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.IconsGray"),
              ElementsAre(base::Bucket(/*min=*/6, /*count=*/1)));
}

TEST(RecordPageImpressionTest, ShouldRecordUmaForThumbnails) {
  base::HistogramTester histogram_tester;
  RecordPageImpression({{TileSource::TOP_SITES, THUMBNAIL_FAILED, GURL()},
                        {TileSource::SUGGESTIONS_SERVICE, THUMBNAIL, GURL()},
                        {TileSource::POPULAR, THUMBNAIL, GURL()}},
                       /*rappor_service=*/nullptr);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.server"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.SuggestionsImpression.client"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.popular"),
              ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL, /*count=*/2),
                          base::Bucket(/*min=*/THUMBNAIL_FAILED, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.server"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL_FAILED, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.popular"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.IconsReal"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.IconsColor"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.SuggestionsImpression.IconsGray"),
              IsEmpty());
}

TEST(RecordTileClickTest, ShouldRecordUmaForIcon) {
  base::HistogramTester histogram_tester;
  RecordTileClick(3, TileSource::TOP_SITES, ICON_REAL);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.popular"),
              IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsReal"),
      ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsColor"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsGray"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.Thumbnail"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.ThumbnailFailed"),
      IsEmpty());
}

TEST(RecordTileClickTest, ShouldRecordUmaForThumbnail) {
  base::HistogramTester histogram_tester;
  RecordTileClick(3, TileSource::TOP_SITES, THUMBNAIL);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.popular"),
              IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsReal"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsColor"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsGray"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.Thumbnail"),
      ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.ThumbnailFailed"),
      IsEmpty());
}

TEST(RecordTileClickTest, ShouldNotRecordUnknownTileType) {
  base::HistogramTester histogram_tester;
  RecordTileClick(3, TileSource::TOP_SITES, UNKNOWN_TILE_TYPE);
  // The click should still get recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.popular"),
              IsEmpty());
  // But all of the tile type histograms should be empty.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsReal"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsColor"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.IconsGray"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.Thumbnail"),
      IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.ThumbnailFailed"),
      IsEmpty());
}

TEST(RecordPageImpressionTest, ShouldRecordRappor) {
  rappor::TestRapporServiceImpl rappor_service;

  RecordPageImpression(
      {{TileSource::TOP_SITES, ICON_REAL, GURL("http://www.site1.com/")},
       {TileSource::TOP_SITES, ICON_COLOR, GURL("http://www.site2.com/")},
       {TileSource::TOP_SITES, ICON_DEFAULT, GURL("http://www.site3.com/")}},
      &rappor_service);

  EXPECT_EQ(3, rappor_service.GetReportsCount());

  {
    std::string sample;
    rappor::RapporType type;
    EXPECT_TRUE(rappor_service.GetRecordedSampleForMetric(
        "NTP.SuggestionsImpressions.IconsReal", &sample, &type));
    EXPECT_EQ("site1.com", sample);
    EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
  }

  {
    std::string sample;
    rappor::RapporType type;
    EXPECT_TRUE(rappor_service.GetRecordedSampleForMetric(
        "NTP.SuggestionsImpressions.IconsColor", &sample, &type));
    EXPECT_EQ("site2.com", sample);
    EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
  }

  {
    std::string sample;
    rappor::RapporType type;
    EXPECT_TRUE(rappor_service.GetRecordedSampleForMetric(
        "NTP.SuggestionsImpressions.IconsGray", &sample, &type));
    EXPECT_EQ("site3.com", sample);
    EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
  }
}

TEST(RecordPageImpressionTest, ShouldNotRecordRapporForUnknownTileType) {
  rappor::TestRapporServiceImpl rappor_service;

  RecordPageImpression(
      {{TileSource::TOP_SITES, ICON_REAL, GURL("http://www.s1.com/")},
       {TileSource::TOP_SITES, UNKNOWN_TILE_TYPE, GURL("http://www.s2.com/")}},
      &rappor_service);

  // Unknown tile type shouldn't get reported.
  EXPECT_EQ(1, rappor_service.GetReportsCount());
}

}  // namespace
}  // namespace metrics
}  // namespace ntp_tiles
