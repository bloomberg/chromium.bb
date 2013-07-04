// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_MODE_H_
#define CC_RESOURCES_RASTER_MODE_H_

#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}

namespace cc {

// Low quality implies no lcd test;
// high quality implies lcd text.
// Note that the order of these matters, from "better" to "worse" in terms of
// quality.
enum RasterMode {
  HIGH_QUALITY_NO_LCD_RASTER_MODE = 0,
  HIGH_QUALITY_RASTER_MODE = 1,
  LOW_QUALITY_RASTER_MODE = 2,
  NUM_RASTER_MODES = 3
};

scoped_ptr<base::Value> RasterModeAsValue(RasterMode mode);

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_MODE_H_
