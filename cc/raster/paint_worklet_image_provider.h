// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_
#define CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_

#include "cc/cc_export.h"
#include "cc/paint/image_provider.h"

namespace cc {
class PaintWorkletImageCache;
class PaintWorkletInput;

// PaintWorkletImageProvider is a bridge between PaintWorkletImageCache and its
// rasterization.
class CC_EXPORT PaintWorkletImageProvider {
 public:
  explicit PaintWorkletImageProvider(PaintWorkletImageCache* cache);
  PaintWorkletImageProvider(const PaintWorkletImageProvider&) = delete;
  PaintWorkletImageProvider(PaintWorkletImageProvider&& other);
  ~PaintWorkletImageProvider();

  PaintWorkletImageProvider& operator=(const PaintWorkletImageProvider&) =
      delete;
  PaintWorkletImageProvider& operator=(PaintWorkletImageProvider&& other);

  ImageProvider::ScopedResult GetPaintRecordResult(PaintWorkletInput* input);

 private:
  PaintWorkletImageCache* cache_;
};

}  // namespace cc

#endif  // CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_
