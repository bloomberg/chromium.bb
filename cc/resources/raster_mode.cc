// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_mode.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace cc {

std::string RasterModeToString(RasterMode raster_mode) {
  switch (raster_mode) {
    case HIGH_QUALITY_RASTER_MODE:
      return "HIGH_QUALITY_RASTER_MODE";
    case LOW_QUALITY_RASTER_MODE:
      return "LOW_QUALITY_RASTER_MODE";
    default:
      NOTREACHED() << "Unrecognized RasterMode value " << raster_mode;
      return "<unknown RasterMode value>";
  }
}

}  // namespace cc
