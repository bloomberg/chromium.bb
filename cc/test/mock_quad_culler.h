// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_QUAD_CULLER_H_
#define CC_TEST_MOCK_QUAD_CULLER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/test/mock_occlusion_tracker.h"

namespace cc {
class LayerImpl;

class MockQuadCuller : public QuadSink {
 public:
  virtual ~MockQuadCuller();

  MockQuadCuller(RenderPass* render_pass,
                 MockOcclusionTracker<LayerImpl>* occlusion_tracker);

  virtual gfx::Rect UnoccludedContentRect(
      const gfx::Rect& content_rect,
      const gfx::Transform& draw_transform) OVERRIDE;

  virtual gfx::Rect UnoccludedContributingSurfaceContentRect(
      const gfx::Rect& content_rect,
      const gfx::Transform& draw_transform) OVERRIDE;

  const QuadList& quad_list() const { return render_pass_->quad_list; }
  const SharedQuadStateList& shared_quad_state_list() const {
    return render_pass_->shared_quad_state_list;
  }

  void set_occluded_target_rect(const gfx::Rect& occluded) {
    occlusion_tracker_->set_occluded_target_rect(occluded);
  }

  void set_occluded_target_rect_for_contributing_surface(
      const gfx::Rect& occluded) {
    occlusion_tracker_->set_occluded_target_rect_for_contributing_surface(
        occluded);
  }

  void clear_lists() {
    render_pass_->quad_list.clear();
    render_pass_->shared_quad_state_list.clear();
  }

 private:
  MockOcclusionTracker<LayerImpl>* occlusion_tracker_;
};

}  // namespace cc

#endif  // CC_TEST_MOCK_QUAD_CULLER_H_
