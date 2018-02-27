// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_DECODE_SURFACE_H_
#define MEDIA_GPU_VAAPI_VAAPI_DECODE_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/va_surface.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// Wrapper of a VASurface with an id and visible area.
class VaapiDecodeSurface
    : public base::RefCountedThreadSafe<VaapiDecodeSurface> {
 public:
  VaapiDecodeSurface(int32_t bitstream_id,
                     const scoped_refptr<VASurface>& va_surface);

  int32_t bitstream_id() const { return bitstream_id_; }
  scoped_refptr<VASurface> va_surface() { return va_surface_; }
  gfx::Rect visible_rect() const { return visible_rect_; }
  void set_visible_rect(const gfx::Rect& visible_rect) {
    visible_rect_ = visible_rect;
  }

 private:
  friend class base::RefCountedThreadSafe<VaapiDecodeSurface>;
  ~VaapiDecodeSurface();

  const int32_t bitstream_id_;
  const scoped_refptr<VASurface> va_surface_;
  gfx::Rect visible_rect_;

  DISALLOW_COPY_AND_ASSIGN(VaapiDecodeSurface);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_DECODE_SURFACE_H_
