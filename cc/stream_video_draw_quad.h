// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STREAM_VIDEO_DRAW_QUAD_H_
#define CC_STREAM_VIDEO_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "ui/gfx/transform.h"

namespace cc {

class CC_EXPORT StreamVideoDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<StreamVideoDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              unsigned texture_id,
              const gfx::Transform& matrix);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              unsigned texture_id,
              const gfx::Transform& matrix);

  unsigned texture_id;
  gfx::Transform matrix;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const StreamVideoDrawQuad* MaterialCast(const DrawQuad*);
 private:
  StreamVideoDrawQuad();
};

}

#endif  // CC_STREAM_VIDEO_DRAW_QUAD_H_
