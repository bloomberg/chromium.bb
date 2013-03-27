// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_PICTURE_DRAW_QUAD_H_
#define CC_QUADS_PICTURE_DRAW_QUAD_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/quads/content_draw_quad_base.h"
#include "cc/resources/picture_pile_impl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/size.h"

namespace cc {

// Used for on-demand tile rasterization.
class CC_EXPORT PictureDrawQuad : public ContentDrawQuadBase {
 public:
  static scoped_ptr<PictureDrawQuad> Create();
  virtual ~PictureDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              const gfx::RectF& tex_coord_rect,
              gfx::Size texture_size,
              bool swizzle_contents,
              gfx::Rect content_rect,
              float contents_scale,
              scoped_refptr<PicturePileImpl> picture_pile);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              gfx::Size texture_size,
              bool swizzle_contents,
              gfx::Rect content_rect,
              float contents_scale,
              scoped_refptr<PicturePileImpl> picture_pile);

  gfx::Rect content_rect;
  float contents_scale;
  scoped_refptr<PicturePileImpl> picture_pile;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const PictureDrawQuad* MaterialCast(const DrawQuad* quad);

 private:
  PictureDrawQuad();
};

}

#endif  // CC_QUADS_PICTURE_DRAW_QUAD_H_
