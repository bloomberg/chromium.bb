// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DRAW_QUAD_H_
#define CC_DRAW_QUAD_H_

#include "cc/cc_export.h"
#include "cc/resource_provider.h"
#include "cc/shared_quad_state.h"

namespace cc {

// DrawQuad is a bag of data used for drawing a quad. Because different
// materials need different bits of per-quad data to render, classes that derive
// from DrawQuad store additional data in their derived instance. The Material
// enum is used to "safely" downcast to the derived class.
class CC_EXPORT DrawQuad {
 public:
  enum Material {
    INVALID,
    CHECKERBOARD,
    DEBUG_BORDER,
    IO_SURFACE_CONTENT,
    RENDER_PASS,
    TEXTURE_CONTENT,
    SOLID_COLOR,
    TILED_CONTENT,
    YUV_VIDEO_CONTENT,
    STREAM_VIDEO_CONTENT,
  };

  virtual ~DrawQuad();

  scoped_ptr<DrawQuad> Copy(
      const SharedQuadState* copied_shared_quad_state) const;

  // TODO(danakj): Chromify or remove these SharedQuadState helpers.
  const gfx::Transform& quadTransform() const { return shared_quad_state->content_to_target_transform; }
  gfx::Rect visibleContentRect() const { return shared_quad_state->visible_content_rect; }
  gfx::Rect clipRect() const { return shared_quad_state->clip_rect; }
  bool isClipped() const { return shared_quad_state->is_clipped; }
  float opacity() const { return shared_quad_state->opacity; }

  Material material;

  // This rect, after applying the quad_transform(), gives the geometry that
  // this quad should draw to.
  gfx::Rect rect;

  // This specifies the region of the quad that is opaque.
  gfx::Rect opaque_rect;

  // Allows changing the rect that gets drawn to make it smaller. This value
  // should be clipped to quadRect.
  gfx::Rect visible_rect;

  // By default blending is used when some part of the quad is not opaque.
  // With this setting, it is possible to force blending on regardless of the
  // opaque area.
  bool needs_blending;

  // Stores state common to a large bundle of quads; kept separate for memory
  // efficiency. There is special treatment to reconstruct these pointers
  // during serialization.
  const SharedQuadState* shared_quad_state;

  bool IsDebugQuad() const { return material == DEBUG_BORDER; }
  bool ShouldDrawWithBlending() const {
    return needs_blending || shared_quad_state->opacity < 1.0f ||
        !opaque_rect.Contains(visible_rect);
  }

  virtual void AppendResources(ResourceProvider::ResourceIdArray* resources);

 protected:
  DrawQuad();

  void SetAll(const SharedQuadState* shared_quad_state,
              Material material,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending);
};

}

#endif  // CC_DRAW_QUAD_H_
