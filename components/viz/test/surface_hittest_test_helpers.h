// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_SURFACE_HITTEST_TEST_HELPERS_H_
#define COMPONENTS_VIZ_TEST_SURFACE_HITTEST_TEST_HELPERS_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "cc/quads/render_pass.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_hittest_delegate.h"
#include "ui/gfx/geometry/insets.h"

namespace gfx {
class Transform;
}

namespace cc {
class CompositorFrame;
}

namespace viz {
namespace test {

void CreateSharedQuadState(cc::RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect);

void CreateSolidColorDrawQuad(cc::RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect);

void CreateRenderPassDrawQuad(cc::RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect,
                              int render_pass_id);

void CreateSurfaceDrawQuad(cc::RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect,
                           const gfx::Rect& quad_rect,
                           SurfaceId surface_id);

void CreateRenderPass(int render_pass_id,
                      const gfx::Rect& rect,
                      const gfx::Transform& transform_to_root_target,
                      cc::RenderPassList* render_pass_list);

cc::CompositorFrame CreateCompositorFrameWithRenderPassList(
    cc::RenderPassList* render_pass_list);

cc::CompositorFrame CreateCompositorFrame(const gfx::Rect& root_rect,
                                          cc::RenderPass** render_pass);

class TestSurfaceHittestDelegate : public SurfaceHittestDelegate {
 public:
  TestSurfaceHittestDelegate();
  ~TestSurfaceHittestDelegate();

  int reject_target_overrides() const { return reject_target_overrides_; }
  int accept_target_overrides() const { return accept_target_overrides_; }

  void AddInsetsForRejectSurface(const SurfaceId& surface_id,
                                 const gfx::Insets& inset);
  void AddInsetsForAcceptSurface(const SurfaceId& surface_id,
                                 const gfx::Insets& inset);

  // SurfaceHittestDelegate implementation.
  bool RejectHitTarget(const cc::SurfaceDrawQuad* surface_quad,
                       const gfx::Point& point_in_quad_space) override;
  bool AcceptHitTarget(const cc::SurfaceDrawQuad* surface_quad,
                       const gfx::Point& point_in_quad_space) override;

 private:
  int reject_target_overrides_;
  int accept_target_overrides_;
  std::map<SurfaceId, gfx::Insets> insets_for_reject_;
  std::map<SurfaceId, gfx::Insets> insets_for_accept_;

  DISALLOW_COPY_AND_ASSIGN(TestSurfaceHittestDelegate);
};

}  // namespace test
}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_SURFACE_HITTEST_TEST_HELPERS_H_
