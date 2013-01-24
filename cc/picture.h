// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_H_
#define CC_PICTURE_H_

#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/rect.h"

namespace cc {

class ContentLayerClient;
struct RenderingStats;

class CC_EXPORT Picture
    : public base::RefCountedThreadSafe<Picture> {
 public:
  static scoped_refptr<Picture> Create(gfx::Rect layer_rect);

  const gfx::Rect& LayerRect() const { return layer_rect_; }
  const gfx::Rect& OpaqueRect() const { return opaque_rect_; }

  // Make a thread-safe clone for rasterizing with.
  scoped_refptr<Picture> Clone() const;

  // Record a paint operation. To be able to safely use this SkPicture for
  // playback on a different thread this can only be called once.
  void Record(ContentLayerClient*, RenderingStats&);

  // Has Record() been called yet?
  bool HasRecording() const { return picture_.get() != NULL; }

  // Apply this contents scale and raster the content rect into the canvas.
  void Raster(SkCanvas* canvas, gfx::Rect content_rect, float contents_scale);

  void GatherPixelRefs(
      const gfx::Rect& layer_rect,
      std::list<skia::LazyPixelRef*>& pixel_ref_list);

 protected:
  Picture(gfx::Rect layer_rect);
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
