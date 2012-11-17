// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DRAW_QUAD_H_
#define CC_DRAW_QUAD_H_

#include "cc/cc_export.h"
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
  const WebKit::WebTransformationMatrix& quadTransform() const { return shared_quad_state_->quadTransform; }
  gfx::Rect visibleContentRect() const { return shared_quad_state_->visibleContentRect; }
  gfx::Rect clippedRectInTarget() const { return shared_quad_state_->clippedRectInTarget; }
  float opacity() const { return shared_quad_state_->opacity; }

  Material material() const { return material_; }

  // This rect, after applying the quad_transform(), gives the geometry that
  // this quad should draw to.
  gfx::Rect rect() const { return rect_; }

  // This specifies the region of the quad that is opaque.
  gfx::Rect opaque_rect() const { return opaque_rect_; }

  // Allows changing the rect that gets drawn to make it smaller. This value
  // should be clipped to quadRect.
  gfx::Rect visible_rect() const { return visible_rect_; }

  // Allows changing the rect that gets drawn to make it smaller. Parameter
  // passed in will be clipped to quadRect().
  void set_visible_rect(gfx::Rect rect) { visible_rect_ = rect; }

  // By default blending is used when some part of the quad is not opaque.
  // With this setting, it is possible to force blending on regardless of the
  // opaque area.
  bool needs_blending() const { return needs_blending_; }

  // Stores state common to a large bundle of quads; kept separate for memory
  // efficiency. There is special treatment to reconstruct these pointers
  // during serialization.
  const SharedQuadState* shared_quad_state() const {
    return shared_quad_state_;
  }

  // Allows changing the rect that gets drawn to make it smaller. Parameter
  // passed in will be clipped to quadRect().
  void set_shared_quad_state(const SharedQuadState* shared_quad_state) {
    shared_quad_state_ = shared_quad_state;
  }

  bool IsDebugQuad() const { return material_ == DEBUG_BORDER; }
  bool ShouldDrawWithBlending() const {
    return needs_blending_ || opacity() < 1.0f ||
        !opaque_rect_.Contains(visible_rect_);
  }

 protected:
  DrawQuad(const SharedQuadState* shared_quad_state,
           Material material,
           gfx::Rect rect,
           gfx::Rect opaque_rect);

  // Stores state common to a large bundle of quads; kept separate for memory
  // efficiency. There is special treatment to reconstruct these pointers
  // during serialization.
  const SharedQuadState* shared_quad_state_;

  Material material_;
  gfx::Rect rect_;
  gfx::Rect visible_rect_;

  // By default blending is used when some part of the quad is not opaque. With
  // this setting, it is possible to force blending on regardless of the opaque
  // area.
  bool needs_blending_;

  // Be default, this rect is empty. It is used when the shared quad state and above
  // variables determine that the quad is not fully opaque but may be partially opaque.
  gfx::Rect opaque_rect_;
};

}

#endif  // CC_DRAW_QUAD_H_
