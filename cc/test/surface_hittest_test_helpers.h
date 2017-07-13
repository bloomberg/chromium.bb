// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SURFACE_HITTEST_TEST_HELPERS_H_
#define CC_TEST_SURFACE_HITTEST_TEST_HELPERS_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "cc/quads/render_pass.h"
#include "cc/surfaces/surface_hittest_delegate.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/gfx/geometry/insets.h"

namespace gfx {
class Transform;
}

namespace cc {

class CompositorFrame;

namespace test {

void CreateSharedQuadState(RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect);

void CreateSolidColorDrawQuad(RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect);

void CreateRenderPassDrawQuad(RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect,
                              int render_pass_id);

void CreateSurfaceDrawQuad(RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect,
                           const gfx::Rect& quad_rect,
                           viz::SurfaceId surface_id);

void CreateRenderPass(int render_pass_id,
                      const gfx::Rect& rect,
                      const gfx::Transform& transform_to_root_target,
                      RenderPassList* render_pass_list);

CompositorFrame CreateCompositorFrameWithRenderPassList(
    RenderPassList* render_pass_list);

CompositorFrame CreateCompositorFrame(const gfx::Rect& root_rect,
                                      RenderPass** render_pass);

class TestSurfaceHittestDelegate : public SurfaceHittestDelegate {
 public:
  TestSurfaceHittestDelegate();
  ~TestSurfaceHittestDelegate();

  int reject_target_overrides() const { return reject_target_overrides_; }
  int accept_target_overrides() const { return accept_target_overrides_; }

  void AddInsetsForRejectSurface(const viz::SurfaceId& surface_id,
                                 const gfx::Insets& inset);
  void AddInsetsForAcceptSurface(const viz::SurfaceId& surface_id,
                                 const gfx::Insets& inset);

  // SurfaceHittestDelegate implementation.
  bool RejectHitTarget(const SurfaceDrawQuad* surface_quad,
                       const gfx::Point& point_in_quad_space) override;
  bool AcceptHitTarget(const SurfaceDrawQuad* surface_quad,
                       const gfx::Point& point_in_quad_space) override;

 private:
  int reject_target_overrides_;
  int accept_target_overrides_;
  std::map<viz::SurfaceId, gfx::Insets> insets_for_reject_;
  std::map<viz::SurfaceId, gfx::Insets> insets_for_accept_;

  DISALLOW_COPY_AND_ASSIGN(TestSurfaceHittestDelegate);
};

}  // namespace test
}  // namespace cc

#endif  // CC_TEST_SURFACE_HITTEST_TEST_HELPERS_H_
