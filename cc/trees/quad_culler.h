// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_QUAD_CULLER_H_
#define CC_TREES_QUAD_CULLER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/render_pass.h"

namespace cc {
class LayerImpl;
class RenderSurfaceImpl;
template <typename LayerType>
class OcclusionTracker;

class CC_EXPORT QuadCuller : public QuadSink {
 public:
  QuadCuller(QuadList* quad_list,
             SharedQuadStateList* shared_quad_state_list,
             const LayerImpl* layer,
             const OcclusionTracker<LayerImpl>& occlusion_tracker,
             bool for_surface);
  virtual ~QuadCuller() {}

  // QuadSink implementation.
  virtual SharedQuadState* UseSharedQuadState(
      scoped_ptr<SharedQuadState> shared_quad_state) OVERRIDE;
  virtual gfx::Rect UnoccludedContentRect(const gfx::Rect& content_rect,
                                          const gfx::Transform& draw_transform)
      OVERRIDE;
  virtual gfx::Rect UnoccludedContributingSurfaceContentRect(
      const gfx::Rect& content_rect,
      const gfx::Transform& draw_transform) OVERRIDE;
  virtual bool MaybeAppend(scoped_ptr<DrawQuad> draw_quad) OVERRIDE;
  virtual void Append(scoped_ptr<DrawQuad> draw_quad) OVERRIDE;

 private:
  QuadList* quad_list_;
  SharedQuadStateList* shared_quad_state_list_;
  const LayerImpl* layer_;
  const OcclusionTracker<LayerImpl>& occlusion_tracker_;

  SharedQuadState* current_shared_quad_state_;
  bool for_surface_;

  DISALLOW_COPY_AND_ASSIGN(QuadCuller);
};

}  // namespace cc

#endif  // CC_TREES_QUAD_CULLER_H_
