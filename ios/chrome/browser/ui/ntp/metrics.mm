// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics.h"

#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "components/rappor/rappor_service_impl.h"
#include "ios/chrome/browser/application_context.h"

namespace {

ntp_tiles::TileVisualType VisualTypeFromAttributes(
    const FaviconAttributes* attributes) {
  if (!attributes) {
    return ntp_tiles::TileVisualType::NONE;
  } else if (attributes.faviconImage) {
    return ntp_tiles::TileVisualType::ICON_REAL;
  }
  return attributes.defaultBackgroundColor
             ? ntp_tiles::TileVisualType::ICON_DEFAULT
             : ntp_tiles::TileVisualType::ICON_COLOR;
}

}  // namespace

void RecordNTPTileImpression(int index,
                             ntp_tiles::TileSource source,
                             ntp_tiles::TileTitleSource title_source,
                             const FaviconAttributes* attributes,
                             base::Time data_generation_time,
                             const GURL& url) {
  ntp_tiles::metrics::RecordTileImpression(
      ntp_tiles::NTPTileImpression(index, source, title_source,
                                   VisualTypeFromAttributes(attributes),
                                   data_generation_time, url),
      GetApplicationContext()->GetRapporServiceImpl());
}

void RecordNTPTileClick(int index,
                        ntp_tiles::TileSource source,
                        ntp_tiles::TileTitleSource title_source,
                        const FaviconAttributes* attributes,
                        base::Time data_generation_time,
                        const GURL& url) {
  ntp_tiles::metrics::RecordTileClick(ntp_tiles::NTPTileImpression(
      index, source, title_source, VisualTypeFromAttributes(attributes),
      data_generation_time, url));
}
