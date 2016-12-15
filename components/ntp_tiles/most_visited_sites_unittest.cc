// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_tiles {

// Defined for googletest. Must be defined in the same namespace.
void PrintTo(const NTPTile& tile, std::ostream* os) {
  *os << "{\"" << tile.title << "\", \"" << tile.url << "\", "
      << static_cast<int>(tile.source) << "}";
}

namespace {

using testing::ElementsAre;

MATCHER_P3(MatchesTile,
           title,
           url,
           source,
           std::string("has title \"") + title + std::string("\" and url \"") +
               url +
               std::string("\" and source ") +
               testing::PrintToString(static_cast<int>(source))) {
  return arg.title == base::ASCIIToUTF16(title) && arg.url == GURL(url) &&
         arg.source == source;
}

NTPTile MakeTile(const std::string& title,
                 const std::string& url,
                 NTPTileSource source) {
  NTPTile tile;
  tile.title = base::ASCIIToUTF16(title);
  tile.url = GURL(url);
  tile.source = source;
  return tile;
}

// This a test for MostVisitedSites::MergeTiles(...) method, and thus has the
// same scope as the method itself. This tests merging popular sites with
// personal tiles.
// More important things out of the scope of testing presently:
// - Removing blacklisted tiles.
// - Correct host extraction from the URL.
// - Ensuring personal tiles are not duplicated in popular tiles.
//
// TODO(mastiz): Add tests for the functionality listed above.
TEST(MostVisitedSitesTest, ShouldMergeTilesWithPersonalOnly) {
  std::vector<NTPTile> personal_tiles{
      MakeTile("Site 1", "https://www.site1.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 2", "https://www.site2.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 3", "https://www.site3.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 4", "https://www.site4.com/", NTPTileSource::TOP_SITES),
  };
  // Without any popular tiles, the result after merge should be the personal
  // tiles.
  EXPECT_THAT(MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                           /*whitelist_tiles=*/NTPTilesVector(),
                                           /*popular_tiles=*/NTPTilesVector()),
              ElementsAre(MatchesTile("Site 1", "https://www.site1.com/",
                                      NTPTileSource::TOP_SITES),
                          MatchesTile("Site 2", "https://www.site2.com/",
                                      NTPTileSource::TOP_SITES),
                          MatchesTile("Site 3", "https://www.site3.com/",
                                      NTPTileSource::TOP_SITES),
                          MatchesTile("Site 4", "https://www.site4.com/",
                                      NTPTileSource::TOP_SITES)));
}

TEST(MostVisitedSitesTest, ShouldMergeTilesWithPopularOnly) {
  std::vector<NTPTile> popular_tiles{
      MakeTile("Site 1", "https://www.site1.com/", NTPTileSource::POPULAR),
      MakeTile("Site 2", "https://www.site2.com/", NTPTileSource::POPULAR),
      MakeTile("Site 3", "https://www.site3.com/", NTPTileSource::POPULAR),
      MakeTile("Site 4", "https://www.site4.com/", NTPTileSource::POPULAR),
  };
  // Without any personal tiles, the result after merge should be the popular
  // tiles.
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(/*personal_tiles=*/NTPTilesVector(),
                                   /*whitelist_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles)),
      ElementsAre(MatchesTile("Site 1", "https://www.site1.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 2", "https://www.site2.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 3", "https://www.site3.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 4", "https://www.site4.com/",
                              NTPTileSource::POPULAR)));
}

TEST(MostVisitedSitesTest, ShouldMergeTilesFavoringPersonalOverPopular) {
  std::vector<NTPTile> popular_tiles{
      MakeTile("Site 1", "https://www.site1.com/", NTPTileSource::POPULAR),
      MakeTile("Site 2", "https://www.site2.com/", NTPTileSource::POPULAR),
  };
  std::vector<NTPTile> personal_tiles{
      MakeTile("Site 3", "https://www.site3.com/", NTPTileSource::TOP_SITES),
      MakeTile("Site 4", "https://www.site4.com/", NTPTileSource::TOP_SITES),
  };
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                   /*whitelist_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles)),
      ElementsAre(MatchesTile("Site 3", "https://www.site3.com/",
                              NTPTileSource::TOP_SITES),
                  MatchesTile("Site 4", "https://www.site4.com/",
                              NTPTileSource::TOP_SITES),
                  MatchesTile("Site 1", "https://www.site1.com/",
                              NTPTileSource::POPULAR),
                  MatchesTile("Site 2", "https://www.site2.com/",
                              NTPTileSource::POPULAR)));
}

}  // namespace
}  // namespace ntp_tiles
