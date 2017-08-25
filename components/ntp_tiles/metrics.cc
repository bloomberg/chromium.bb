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
const char kHistogramPopularName[] = "popular_fetched";
const char kHistogramBakedInName[] = "popular_baked_in";
const char kHistogramWhitelistName[] = "whitelist";
const char kHistogramHomepageName[] = "homepage";

// Suffixes for the various icon types.
const char kTileTypeSuffixIconColor[] = "IconsColor";
const char kTileTypeSuffixIconGray[] = "IconsGray";
const char kTileTypeSuffixIconReal[] = "IconsReal";
const char kTileTypeSuffixThumbnail[] = "Thumbnail";
const char kTileTypeSuffixThumbnailFailed[] = "ThumbnailFailed";

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

std::string GetSourceHistogramName(TileSource source) {
  switch (source) {
    case TileSource::TOP_SITES:
      return kHistogramClientName;
    case TileSource::POPULAR_BAKED_IN:
      return kHistogramBakedInName;
    case TileSource::POPULAR:
      return kHistogramPopularName;
    case TileSource::WHITELIST:
      return kHistogramWhitelistName;
    case TileSource::SUGGESTIONS_SERVICE:
      return kHistogramServerName;
    case TileSource::HOMEPAGE:
      return kHistogramHomepageName;
  }
  NOTREACHED();
  return std::string();
}

const char* GetTileTypeSuffix(TileVisualType type) {
  switch (type) {
    case TileVisualType::ICON_COLOR:
      return kTileTypeSuffixIconColor;
    case TileVisualType::ICON_DEFAULT:
      return kTileTypeSuffixIconGray;
    case TileVisualType::ICON_REAL:
      return kTileTypeSuffixIconReal;
    case THUMBNAIL:
      return kTileTypeSuffixThumbnail;
    case THUMBNAIL_FAILED:
      return kTileTypeSuffixThumbnailFailed;
    case TileVisualType::NONE:                     // Fall through.
    case TileVisualType::UNKNOWN_TILE_TYPE:
      break;
  }
  return nullptr;
}

}  // namespace

void RecordPageImpression(int number_of_tiles) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.NumberOfTiles", number_of_tiles);
}

void RecordTileImpression(int index,
                          TileSource source,
                          TileVisualType type,
                          const GURL& url,
                          rappor::RapporService* rappor_service) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SuggestionsImpression", index,
                            kMaxNumTiles);

  std::string source_name = GetSourceHistogramName(source);
  std::string impression_histogram = base::StringPrintf(
      "NewTabPage.SuggestionsImpression.%s", source_name.c_str());
  LogHistogramEvent(impression_histogram, index, kMaxNumTiles);

  if (type > LAST_RECORDED_TILE_TYPE) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileType", type,
                            LAST_RECORDED_TILE_TYPE + 1);

  std::string tile_type_histogram =
      base::StringPrintf("NewTabPage.TileType.%s", source_name.c_str());
  LogHistogramEvent(tile_type_histogram, type, LAST_RECORDED_TILE_TYPE + 1);

  const char* tile_type_suffix = GetTileTypeSuffix(type);
  if (tile_type_suffix) {
    // Note: This handles a null |rappor_service|.
    rappor::SampleDomainAndRegistryFromGURL(
        rappor_service,
        base::StringPrintf("NTP.SuggestionsImpressions.%s", tile_type_suffix),
        url);

    std::string icon_impression_histogram = base::StringPrintf(
        "NewTabPage.SuggestionsImpression.%s", tile_type_suffix);
    LogHistogramEvent(icon_impression_histogram, index, kMaxNumTiles);
  }
}

void RecordTileClick(int index, TileSource source, TileVisualType tile_type) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", index, kMaxNumTiles);

  std::string histogram = base::StringPrintf(
      "NewTabPage.MostVisited.%s", GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, index, kMaxNumTiles);

  const char* tile_type_suffix = GetTileTypeSuffix(tile_type);
  if (tile_type_suffix) {
    std::string tile_type_histogram =
        base::StringPrintf("NewTabPage.MostVisited.%s", tile_type_suffix);
    LogHistogramEvent(tile_type_histogram, index, kMaxNumTiles);
  }

  if (tile_type <= LAST_RECORDED_TILE_TYPE) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileTypeClicked", tile_type,
                              LAST_RECORDED_TILE_TYPE + 1);

    std::string histogram =
        base::StringPrintf("NewTabPage.TileTypeClicked.%s",
                           GetSourceHistogramName(source).c_str());
    LogHistogramEvent(histogram, tile_type, LAST_RECORDED_TILE_TYPE + 1);
  }
}

}  // namespace metrics
}  // namespace ntp_tiles
