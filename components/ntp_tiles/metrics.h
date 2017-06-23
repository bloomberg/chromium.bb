// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_METRICS_H_
#define COMPONENTS_NTP_TILES_METRICS_H_

#include <utility>
#include <vector>

#include "components/ntp_tiles/ntp_tile.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "url/gurl.h"

namespace rappor {
class RapporService;
}  // namespace rappor

namespace ntp_tiles {
namespace metrics {

// Records an NTP impression, after all tiles have loaded.
void RecordPageImpression(int number_of_tiles);

// Records a tile impression at |index| (zero based) created by |source|. This
// should be called only after the visual |type| of the tile has been
// determined. If |rappor_service| is null, no rappor metrics will be reported.
void RecordTileImpression(int index,
                          TileSource source,
                          TileVisualType type,
                          const GURL& url,
                          rappor::RapporService* rappor_service);

// Records a click on a tile.
void RecordTileClick(int index, TileSource source, TileVisualType tile_type);

}  // namespace metrics
}  // namespace ntp_tiles

#endif
