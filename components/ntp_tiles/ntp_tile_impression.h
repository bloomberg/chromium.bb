// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_IMPRESSION_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_IMPRESSION_H_

#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_title_source.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "url/gurl.h"

namespace ntp_tiles {

struct NTPTileImpression {
  // Default constructor needed for Mojo.
  NTPTileImpression();
  NTPTileImpression(int index,
                    TileSource source,
                    TileTitleSource title_source,
                    TileVisualType visual_type,
                    const GURL& url_for_rappor);
  ~NTPTileImpression();

  // Zero-based index representing the position.
  int index;
  TileSource source;
  TileTitleSource title_source;
  TileVisualType visual_type;
  // URL the tile points to, used to report Rappor metrics only (might be empty
  // and is hence ignored, e.g. on desktop).
  GURL url_for_rappor;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_IMPRESSION_H_
