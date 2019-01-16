// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_
#define CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_

#include "cc/cc_export.h"
#include "cc/paint/image_provider.h"

namespace cc {
class PaintWorkletImageCache;

// PaintWorkletImageProvider is a bridge between PaintWorkletImageCache and its
// rasterization.
class CC_EXPORT PaintWorkletImageProvider {
 public:
  explicit PaintWorkletImageProvider(PaintWorkletImageCache* cache);
  ~PaintWorkletImageProvider();

  PaintWorkletImageProvider(PaintWorkletImageProvider&& other);
  PaintWorkletImageProvider& operator=(PaintWorkletImageProvider&& other);

 private:
  PaintWorkletImageCache* cache_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletImageProvider);
};

}  // namespace cc

#endif  // CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_
