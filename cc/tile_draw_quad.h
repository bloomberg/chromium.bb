// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_DRAW_QUAD_H_
#define CC_TILE_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT TileDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<TileDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              unsigned resource_id,
              const gfx::RectF& tex_coord_rect,
              gfx::Size texture_size,
              bool swizzle_contents);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              unsigned resource_id,
              const gfx::RectF& tex_coord_rect,
              gfx::Size texture_size,
              bool swizzle_contents);

  unsigned resource_id;
  gfx::RectF tex_coord_rect;
  gfx::Size texture_size;
  bool swizzle_contents;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const TileDrawQuad* MaterialCast(const DrawQuad*);
 private:
  TileDrawQuad();
};

}

#endif  // CC_TILE_DRAW_QUAD_H_
