// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_YUV_VIDEO_DRAW_QUAD_H_
#define CC_QUADS_YUV_VIDEO_DRAW_QUAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/quads/draw_quad.h"

namespace cc {

class CC_EXPORT YUVVideoDrawQuad : public DrawQuad {
 public:
  enum ColorSpace {
    REC_601,  // SDTV standard with restricted "studio swing" color range.
    REC_709,  // HDTV standard with restricted "studio swing" color range.
    JPEG,     // Full color range [0, 255] JPEG color space.
    COLOR_SPACE_LAST = JPEG
  };

  ~YUVVideoDrawQuad() override;

  YUVVideoDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& min_tex_size,
              unsigned y_plane_resource_id,
              unsigned u_plane_resource_id,
              unsigned v_plane_resource_id,
              unsigned a_plane_resource_id,
              ColorSpace color_space);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& min_tex_size,
              unsigned y_plane_resource_id,
              unsigned u_plane_resource_id,
              unsigned v_plane_resource_id,
              unsigned a_plane_resource_id,
              ColorSpace color_space);

  gfx::RectF tex_coord_rect;
  // Empty texture size implies no clamping of texture coordinates.
  gfx::Size min_tex_size;
  unsigned y_plane_resource_id;
  unsigned u_plane_resource_id;
  unsigned v_plane_resource_id;
  unsigned a_plane_resource_id;
  ColorSpace color_space;

  void IterateResources(const ResourceIteratorCallback& callback) override;

  static const YUVVideoDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace cc

#endif  // CC_QUADS_YUV_VIDEO_DRAW_QUAD_H_
