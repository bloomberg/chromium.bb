// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_BORDER_DRAW_QUAD_H_
#define CC_DEBUG_BORDER_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class CC_EXPORT DebugBorderDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<DebugBorderDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              SkColor color,
              int width);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              SkColor color,
              int width);

  SkColor color;
  int width;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const DebugBorderDrawQuad* MaterialCast(const DrawQuad*);
 private:
  DebugBorderDrawQuad();
};

}

#endif  // CC_DEBUG_BORDER_DRAW_QUAD_H_
