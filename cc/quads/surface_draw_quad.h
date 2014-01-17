// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_SURFACE_DRAW_QUAD_H_
#define CC_QUADS_SURFACE_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/quads/draw_quad.h"

namespace cc {

class CC_EXPORT SurfaceDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<SurfaceDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              int surface_id);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              int surface_id);

  int surface_id;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const SurfaceDrawQuad* MaterialCast(const DrawQuad* quad);

 private:
  SurfaceDrawQuad();
  virtual void ExtendValue(base::DictionaryValue* value) const OVERRIDE;
};

}  // namespace cc

#endif  // CC_QUADS_SURFACE_DRAW_QUAD_H_
