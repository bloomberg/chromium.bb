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

constexpr int kUnknownTitleSource = static_cast<int>(TileTitleSource::UNKNOWN);
constexpr int kManifestTitleSource =
    static_cast<int>(TileTitleSource::MANIFEST);
constexpr int kMetaTagTitleSource = static_cast<int>(TileTitleSource::META_TAG);
constexpr int kTitleTagTitleSource =
    static_cast<int>(TileTitleSource::TITLE_TAG);
constexpr int kInferredTitleSource =
    static_cast<int>(TileTitleSource::INFERRED);

using testing::ElementsAre;
using testing::IsEmpty;

TEST(RecordPageImpressionTest, ShouldRecordNumberOfTiles) {
  base::HistogramTester histogram_tester;
  RecordPageImpression(5);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.NumberOfTiles"),
              ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
}

TEST(RecordTileImpressionTest, ShouldRecordUmaForIcons) {
  base::HistogramTester histogram_tester;

  RecordTileImpression(0, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_REAL, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(1, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_REAL, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(2, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_REAL, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(3, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_COLOR, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(4, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_COLOR, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(5, TileTitleSource::UNKNOWN,
                       TileSource::SUGGESTIONS_SERVICE, ICON_REAL, GURL(),
                       /*rappor_service=*/nullptr);
  RecordTileImpression(6, TileTitleSource::UNKNOWN,
                       TileSource::SUGGESTIONS_SERVICE, ICON_DEFAULT, GURL(),
                       /*rappor_service=*/nullptr);
  RecordTileImpression(7, TileTitleSource::TITLE_TAG, TileSource::POPULAR,
                       ICON_COLOR, GURL(), /*rappor_service=*/nullptr);

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
                  "NewTabPage.SuggestionsImpression.popular_fetched"),
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
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.popular_fetched"),
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

TEST(RecordTileImpressionTest, ShouldRecordUmaForThumbnails) {
  base::HistogramTester histogram_tester;

  RecordTileImpression(0, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       THUMBNAIL_FAILED, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(1, TileTitleSource::UNKNOWN,
                       TileSource::SUGGESTIONS_SERVICE, THUMBNAIL, GURL(),
                       /*rappor_service=*/nullptr);
  RecordTileImpression(2, TileTitleSource::TITLE_TAG, TileSource::POPULAR,
                       THUMBNAIL, GURL(), /*rappor_service=*/nullptr);

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
                  "NewTabPage.SuggestionsImpression.popular_fetched"),
              ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL, /*count=*/2),
                          base::Bucket(/*min=*/THUMBNAIL_FAILED, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.server"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileType.client"),
              ElementsAre(base::Bucket(/*min=*/THUMBNAIL_FAILED, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileType.popular_fetched"),
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

TEST(RecordTileImpressionTest, ShouldRecordImpressionsForTileTitleSource) {
  base::HistogramTester histogram_tester;
  RecordTileImpression(0, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_REAL, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(1, TileTitleSource::INFERRED,
                       TileSource::SUGGESTIONS_SERVICE, ICON_REAL, GURL(),
                       /*rappor_service=*/nullptr);
  RecordTileImpression(2, TileTitleSource::TITLE_TAG, TileSource::POPULAR,
                       ICON_REAL, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(3, TileTitleSource::MANIFEST, TileSource::POPULAR,
                       ICON_REAL, GURL(), /*rappor_service=*/nullptr);
  RecordTileImpression(4, TileTitleSource::TITLE_TAG,
                       TileSource::POPULAR_BAKED_IN, ICON_REAL, GURL(),
                       /*rappor_service=*/nullptr);
  RecordTileImpression(5, TileTitleSource::META_TAG,
                       TileSource::POPULAR_BAKED_IN, ICON_REAL, GURL(),
                       /*rappor_service=*/nullptr);

  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle.client"),
              ElementsAre(base::Bucket(kUnknownTitleSource, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle.server"),
              ElementsAre(base::Bucket(kInferredTitleSource, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitle.popular_fetched"),
      ElementsAre(base::Bucket(kManifestTitleSource, /*count=*/1),
                  base::Bucket(kTitleTagTitleSource, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitle.popular_baked_in"),
      ElementsAre(base::Bucket(kMetaTagTitleSource, /*count=*/1),
                  base::Bucket(kTitleTagTitleSource, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitle"),
              ElementsAre(base::Bucket(kUnknownTitleSource, /*count=*/1),
                          base::Bucket(kManifestTitleSource, /*count=*/1),
                          base::Bucket(kMetaTagTitleSource, /*count=*/1),
                          base::Bucket(kTitleTagTitleSource, /*count=*/2),
                          base::Bucket(kInferredTitleSource, /*count=*/1)));
}

TEST(RecordTileClickTest, ShouldRecordUmaForIcon) {
  base::HistogramTester histogram_tester;
  RecordTileClick(3, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                  ICON_REAL);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.popular_fetched"),
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
  RecordTileClick(3, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                  THUMBNAIL);
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.popular_fetched"),
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
  RecordTileClick(3, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                  UNKNOWN_TILE_TYPE);
  // The click should still get recorded.
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.client"),
              ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.MostVisited.server"),
              IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.MostVisited.popular_fetched"),
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

TEST(RecordTileImpressionTest, ShouldRecordRappor) {
  rappor::TestRapporServiceImpl rappor_service;

  RecordTileImpression(0, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_REAL, GURL("http://www.site1.com/"),
                       &rappor_service);
  RecordTileImpression(1, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_COLOR, GURL("http://www.site2.com/"),
                       &rappor_service);
  RecordTileImpression(2, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_DEFAULT, GURL("http://www.site3.com/"),
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

TEST(RecordTileImpressionTest, ShouldNotRecordRapporForUnknownTileType) {
  rappor::TestRapporServiceImpl rappor_service;

  RecordTileImpression(0, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       ICON_REAL, GURL("http://www.s1.com/"), &rappor_service);
  RecordTileImpression(1, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                       UNKNOWN_TILE_TYPE, GURL("http://www.s2.com/"),
                       &rappor_service);

  // Unknown tile type shouldn't get reported.
  EXPECT_EQ(1, rappor_service.GetReportsCount());
}

TEST(RecordTileClickTest, ShouldRecordClicksForTileTitleSource) {
  base::HistogramTester histogram_tester;
  RecordTileClick(0, TileTitleSource::UNKNOWN, TileSource::TOP_SITES,
                  ICON_REAL);
  RecordTileClick(1, TileTitleSource::UNKNOWN, TileSource::SUGGESTIONS_SERVICE,
                  ICON_REAL);
  RecordTileClick(2, TileTitleSource::TITLE_TAG, TileSource::POPULAR,
                  ICON_REAL);
  RecordTileClick(3, TileTitleSource::MANIFEST, TileSource::POPULAR, ICON_REAL);
  RecordTileClick(4, TileTitleSource::TITLE_TAG, TileSource::POPULAR_BAKED_IN,
                  ICON_REAL);
  RecordTileClick(5, TileTitleSource::META_TAG, TileSource::POPULAR_BAKED_IN,
                  ICON_REAL);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked.client"),
      ElementsAre(base::Bucket(kUnknownTitleSource, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked.server"),
      ElementsAre(base::Bucket(kUnknownTitleSource, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.TileTitleClicked.popular_fetched"),
              ElementsAre(base::Bucket(kManifestTitleSource, /*count=*/1),
                          base::Bucket(kTitleTagTitleSource, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.TileTitleClicked.popular_baked_in"),
              ElementsAre(base::Bucket(kMetaTagTitleSource, /*count=*/1),
                          base::Bucket(kTitleTagTitleSource, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("NewTabPage.TileTitleClicked"),
              ElementsAre(base::Bucket(kUnknownTitleSource, /*count=*/2),
                          base::Bucket(kManifestTitleSource, /*count=*/1),
                          base::Bucket(kMetaTagTitleSource, /*count=*/1),
                          base::Bucket(kTitleTagTitleSource, /*count=*/2)));
}

}  // namespace
}  // namespace metrics
}  // namespace ntp_tiles
