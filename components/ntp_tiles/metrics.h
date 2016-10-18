// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_METRICS_H_
#define COMPONENTS_NTP_TILES_METRICS_H_

#include <vector>

#include "components/ntp_tiles/ntp_tile.h"

namespace ntp_tiles {
namespace metrics {

// The visual type of a most visited tile.
//
// These values must stay in sync with the MostVisitedTileType enum
// in histograms.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
enum MostVisitedTileType {
  // The icon or thumbnail hasn't loaded yet.
  NONE,
  // The item displays a site's actual favicon or touch icon.
  ICON_REAL,
  // The item displays a color derived from the site's favicon or touch icon.
  ICON_COLOR,
  // The item displays a default gray box in place of an icon.
  ICON_DEFAULT,
  NUM_TILE_TYPES,
};

// TODO(treib): Split into individual impressions for desktop.
void RecordImpressions(const NTPTilesVector& tiles);

void RecordImpressionTileTypes(
    const std::vector<MostVisitedTileType>& tile_types,
    const std::vector<NTPTileSource>& sources);

// TODO(treib): Split into "base" click and tile type for desktop.
void RecordClick(int index,
                 MostVisitedTileType tile_type,
                 NTPTileSource source);

}  // namespace metrics
}  // namespace ntp_tiles

#endif
