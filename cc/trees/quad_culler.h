// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_QUAD_CULLER_H_
#define CC_TREES_QUAD_CULLER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/quad_sink.h"

namespace cc {
class LayerImpl;
class RenderPass;
class RenderSurfaceImpl;
template <typename LayerType>
class OcclusionTracker;

class CC_EXPORT QuadCuller : public QuadSink {
 public:
  QuadCuller(RenderPass* render_pass,
             const LayerImpl* layer,
             const OcclusionTracker<LayerImpl>& occlusion_tracker);
  virtual ~QuadCuller() {}

  // QuadSink implementation.
  virtual SharedQuadState* CreateSharedQuadState() OVERRIDE;
  virtual gfx::Rect UnoccludedContentRect(const gfx::Rect& content_rect,
                                          const gfx::Transform& draw_transform)
      OVERRIDE;
  virtual gfx::Rect UnoccludedContributingSurfaceContentRect(
      const gfx::Rect& content_rect,
      const gfx::Transform& draw_transform) OVERRIDE;
  virtual void Append(scoped_ptr<DrawQuad> draw_quad) OVERRIDE;

 private:
  RenderPass* render_pass_;
  const LayerImpl* layer_;
  const OcclusionTracker<LayerImpl>& occlusion_tracker_;

  SharedQuadState* current_shared_quad_state_;

  DISALLOW_COPY_AND_ASSIGN(QuadCuller);
};

}  // namespace cc

#endif  // CC_TREES_QUAD_CULLER_H_
