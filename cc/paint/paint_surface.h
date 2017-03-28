// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_SURFACE_H_
#define CC_PAINT_PAINT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skia_paint_canvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cc {

class CC_PAINT_EXPORT PaintSurface : public SkRefCntBase {
 public:
  explicit PaintSurface(sk_sp<SkSurface> surface);
  ~PaintSurface() override;

  // TODO(enne): this interface matches SkSurface for simplicity.
  // However, this should really be changed to make the SkSurface ctor
  // public and have that be the only constructor.  This will also
  // clean up a bunch of code where there's an SkSurface and a PaintCanvas,
  // such as for canvas in Blink.
  static sk_sp<PaintSurface> MakeRaster(const SkImageInfo& info,
                                        const SkSurfaceProps* props = nullptr) {
    sk_sp<SkSurface> s = SkSurface::MakeRaster(info, props);
    return sk_make_sp<PaintSurface>(std::move(s));
  }
  static sk_sp<PaintSurface> MakeRasterN32Premul(
      int width,
      int height,
      const SkSurfaceProps* props = nullptr) {
    sk_sp<SkSurface> s = SkSurface::MakeRasterN32Premul(width, height, props);
    return sk_make_sp<PaintSurface>(std::move(s));
  }

  int width() const { return surface_->width(); }
  int height() const { return surface_->height(); }

  PaintCanvas* getCanvas() {
    if (!canvas_.has_value())
      canvas_.emplace(surface_->getCanvas());
    return &canvas_.value();
  }

 private:
  const sk_sp<SkSurface> surface_;
  base::Optional<SkiaPaintCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(PaintSurface);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_SURFACE_H_
