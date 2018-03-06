// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_MANAGER_SETTINGS_H_
#define CC_TILES_TILE_MANAGER_SETTINGS_H_

#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT TileManagerSettings {
  bool use_partial_raster = false;
  bool enable_checker_imaging = false;
  size_t min_image_bytes_to_checker = 1 * 1024 * 1024;
};

}  // namespace cc

#endif  // CC_TILES_TILE_MANAGER_SETTINGS_H_
