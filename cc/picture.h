// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_H_
#define CC_PICTURE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect.h"

namespace cc {

class ContentLayerClient;
struct RenderingStats;

class CC_EXPORT Picture
    : public base::RefCountedThreadSafe<Picture> {
public:
  static scoped_refptr<Picture> Create();

  const gfx::Rect& LayerRect() const { return layer_rect_; }
  const gfx::Rect& OpaqueRect() const { return opaque_rect_; }

  // Make a thread-safe clone for rasterizing with.
  scoped_refptr<Picture> Clone();

  // Record a paint operation. To be able to safely use this SkPicture for
  // playback on a different thread this can only be called once.
  void Record(ContentLayerClient*, gfx::Rect layer_rect, RenderingStats&);

  // Raster this Picture's layer_rect into the given canvas.
  // Assumes contentsScale have already been applied.
  void Raster(SkCanvas* canvas);

private:
  Picture();
  // This constructor assumes SkPicture is already ref'd and transfers
  // ownership to this picture.
  Picture(const skia::RefPtr<SkPicture>&,
          gfx::Rect layer_rect,
          gfx::Rect opaque_rect);
  ~Picture();

  gfx::Rect layer_rect_;
  gfx::Rect opaque_rect_;
  skia::RefPtr<SkPicture> picture_;

  friend class base::RefCountedThreadSafe<Picture>;
  DISALLOW_COPY_AND_ASSIGN(Picture);
};

}  // namespace cc

#endif  // CC_PICTURE_H_
