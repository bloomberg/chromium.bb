// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_BUFFER_H_
#define CC_RESOURCES_RASTER_BUFFER_H_

#include "cc/base/cc_export.h"
#include "skia/ext/refptr.h"

class SkCanvas;

namespace cc {

class CC_EXPORT RasterBuffer {
 public:
  RasterBuffer();
  virtual ~RasterBuffer();

  virtual skia::RefPtr<SkCanvas> AcquireSkCanvas() = 0;
  virtual void ReleaseSkCanvas(const skia::RefPtr<SkCanvas>& canvas) = 0;
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_BUFFER_H_
