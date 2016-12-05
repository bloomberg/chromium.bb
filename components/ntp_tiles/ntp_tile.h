// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/ntp_tiles/ntp_tile_source.h"
#include "url/gurl.h"

namespace ntp_tiles {

// A suggested site shown on the New Tab Page.
struct NTPTile {
  base::string16 title;
  GURL url;
  NTPTileSource source;

  // Only valid for source == WHITELIST (empty otherwise).
  base::FilePath whitelist_icon_path;

  NTPTile();
  NTPTile(const NTPTile&);
  ~NTPTile();
};

using NTPTilesVector = std::vector<NTPTile>;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_H_
