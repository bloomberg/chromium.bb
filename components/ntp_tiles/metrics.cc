// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/metrics.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "components/rappor/public/rappor_utils.h"

namespace ntp_tiles {
namespace metrics {

namespace {

// Maximum number of tiles to record in histograms.
const int kMaxNumTiles = 12;

// Identifiers for the various tile sources.
const char kHistogramClientName[] = "client";
const char kHistogramServerName[] = "server";
const char kHistogramPopularName[] = "popular";
const char kHistogramWhitelistName[] = "whitelist";

// Suffixes for the various icon types.
const char kIconTypeSuffixColor[] = "IconsColor";
const char kIconTypeSuffixGray[] = "IconsGray";
const char kIconTypeSuffixReal[] = "IconsReal";

// Log an event for a given |histogram| at a given element |position|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogHistogramEvent(const std::string& histogram,
                       int position,
                       int num_sites) {
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram, 1, num_sites, num_sites + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->Add(position);
}

std::string GetSourceHistogramName(NTPTileSource source) {
  switch (source) {
    case NTPTileSource::TOP_SITES:
      return kHistogramClientName;
    case NTPTileSource::POPULAR:
      return kHistogramPopularName;
    case NTPTileSource::WHITELIST:
      return kHistogramWhitelistName;
    case NTPTileSource::SUGGESTIONS_SERVICE:
      return kHistogramServerName;
  }
  NOTREACHED();
  return std::string();
}

const char* GetIconTypeSuffix(MostVisitedTileType type) {
  switch (type) {
    case ICON_COLOR:
      return kIconTypeSuffixColor;
    case ICON_DEFAULT:
      return kIconTypeSuffixGray;
    case ICON_REAL:
      return kIconTypeSuffixReal;
    case NONE:                     // Fall through.
    case NUM_RECORDED_TILE_TYPES:  // Fall through.
    case THUMBNAIL:                // Fall through.
    case UNKNOWN_TILE_TYPE:
      break;
  }
  return nullptr;
}

}  // namespace

void RecordPageImpression(const std::vector<TileImpression>& tiles,
                          rappor::RapporService* rappor_service) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.NumberOfTiles", tiles.size());

  int counts_per_type[NUM_RECORDED_TILE_TYPES] = {0};
  bool have_tile_types = false;
  for (int index = 0; index < static_cast<int>(tiles.size()); index++) {
    NTPTileSource source = tiles[index].source;
    MostVisitedTileType tile_type = tiles[index].type;
    const GURL& url = tiles[index].url;

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.SuggestionsImpression", index,
                              kMaxNumTiles);

    std::string source_name = GetSourceHistogramName(source);
    std::string impression_histogram = base::StringPrintf(
        "NewTabPage.SuggestionsImpression.%s", source_name.c_str());
    LogHistogramEvent(impression_histogram, index, kMaxNumTiles);

    if (tile_type >= NUM_RECORDED_TILE_TYPES) {
      continue;
    }

    have_tile_types = true;
    ++counts_per_type[tile_type];

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileType", tile_type,
                              NUM_RECORDED_TILE_TYPES);

    std::string tile_type_histogram =
        base::StringPrintf("NewTabPage.TileType.%s", source_name.c_str());
    LogHistogramEvent(tile_type_histogram, tile_type, NUM_RECORDED_TILE_TYPES);

    const char* icon_type_suffix = GetIconTypeSuffix(tile_type);
    if (icon_type_suffix) {
      rappor::SampleDomainAndRegistryFromGURL(
          rappor_service,
          base::StringPrintf("NTP.SuggestionsImpressions.%s", icon_type_suffix),
          url);

      std::string icon_impression_histogram = base::StringPrintf(
          "NewTabPage.SuggestionsImpression.%s", icon_type_suffix);
      LogHistogramEvent(icon_impression_histogram, index, kMaxNumTiles);
    }
  }

  if (have_tile_types) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsReal",
                                counts_per_type[ICON_REAL]);
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsColor",
                                counts_per_type[ICON_COLOR]);
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsGray",
                                counts_per_type[ICON_DEFAULT]);
  }
}

void RecordTileClick(int index,
                     NTPTileSource source,
                     MostVisitedTileType tile_type) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", index, kMaxNumTiles);

  std::string histogram = base::StringPrintf(
      "NewTabPage.MostVisited.%s", GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, index, kMaxNumTiles);

  const char* icon_type_suffix = GetIconTypeSuffix(tile_type);
  if (icon_type_suffix) {
    std::string icon_histogram =
        base::StringPrintf("NewTabPage.MostVisited.%s", icon_type_suffix);
    LogHistogramEvent(icon_histogram, index, kMaxNumTiles);
  }

  if (tile_type < NUM_RECORDED_TILE_TYPES) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileTypeClicked", tile_type,
                              NUM_RECORDED_TILE_TYPES);

    std::string histogram =
        base::StringPrintf("NewTabPage.TileTypeClicked.%s",
                           GetSourceHistogramName(source).c_str());
    LogHistogramEvent(histogram, tile_type, NUM_RECORDED_TILE_TYPES);
  }
}

}  // namespace metrics
}  // namespace ntp_tiles
