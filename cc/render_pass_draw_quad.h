// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RENDER_PASS_DRAW_QUAD_H_
#define CC_RENDER_PASS_DRAW_QUAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "cc/render_pass.h"
#include "cc/resource_provider.h"

namespace cc {

class CC_EXPORT RenderPassDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<RenderPassDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              RenderPass::Id render_pass_id,
              bool is_replica,
              ResourceProvider::ResourceId mask_resource_id,
              gfx::Rect contents_changed_since_last_frame,
              gfx::RectF mask_uv_rect);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              RenderPass::Id render_pass_id,
              bool is_replica,
              ResourceProvider::ResourceId mask_resource_id,
              gfx::Rect contents_changed_since_last_frame,
              gfx::RectF mask_uv_rect);

  scoped_ptr<RenderPassDrawQuad> Copy(
      const SharedQuadState* copied_shared_quad_state,
      RenderPass::Id copied_render_pass_id) const;

  RenderPass::Id render_pass_id;
  bool is_replica;
  ResourceProvider::ResourceId mask_resource_id;
  gfx::Rect contents_changed_since_last_frame;
  gfx::RectF mask_uv_rect;

  static const RenderPassDrawQuad* MaterialCast(const DrawQuad*);
private:
  RenderPassDrawQuad();
};

}

#endif  // CC_RENDER_PASS_DRAW_QUAD_H_
