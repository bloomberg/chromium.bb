// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_SOLID_COLOR_DRAW_QUAD_H_
#define CC_QUADS_SOLID_COLOR_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/quads/draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class CC_EXPORT SolidColorDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<SolidColorDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              SkColor color);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              SkColor color);

  SkColor color;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const SolidColorDrawQuad* MaterialCast(const DrawQuad*);

 private:
  SolidColorDrawQuad();
};

}  // namespace cc

#endif  // CC_QUADS_SOLID_COLOR_DRAW_QUAD_H_
