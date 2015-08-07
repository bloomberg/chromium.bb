// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_id.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_hittest.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class EmptySurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  void ReturnResources(const ReturnedResourceArray& resources) override {}
};

TEST(SurfaceHittestTest, Hittest_SingleSurface) {
  SurfaceManager manager;
  EmptySurfaceFactoryClient client;
  SurfaceFactory factory(&manager, &client);
  SurfaceIdAllocator root_allocator(2);

  // Creates a root surface.
  SurfaceId root_surface_id = root_allocator.GenerateId();
  factory.Create(root_surface_id);
  gfx::Rect rect(300, 300);
  RenderPassId id(1, 1);
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());
  scoped_ptr<DelegatedFrameData> delegated_frame_data(new DelegatedFrameData);
  delegated_frame_data->render_pass_list.push_back(pass.Pass());
  scoped_ptr<CompositorFrame> root_frame(new CompositorFrame);
  root_frame->delegated_frame_data = delegated_frame_data.Pass();
  factory.SubmitFrame(root_surface_id, root_frame.Pass(),
                      SurfaceFactory::DrawCallback());

  {
    SurfaceHittest hittest(&manager);
    gfx::Point transformed_point;
    EXPECT_EQ(root_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(100, 100),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(100, 100), transformed_point);
  }

  factory.Destroy(root_surface_id);
}

TEST(SurfaceHittestTest, Hittest_ChildSurface) {
  SurfaceManager manager;
  EmptySurfaceFactoryClient client;
  SurfaceFactory factory(&manager, &client);
  SurfaceIdAllocator root_allocator(2);
  SurfaceIdAllocator child_allocator(3);

  SurfaceId root_surface_id = root_allocator.GenerateId();
  SurfaceId child_surface_id = child_allocator.GenerateId();
  gfx::Size root_size(300, 300);
  gfx::Rect root_rect(root_size);
  gfx::Size child_size(200, 200);
  gfx::Rect child_rect(child_size);
  gfx::Rect child_solid_quad_size(100, 100);
  gfx::Rect child_solid_quad_rect(child_solid_quad_size);

  // Creates a root surface.
  factory.Create(root_surface_id);
  RenderPassId root_id(1, 1);
  scoped_ptr<RenderPass> root_pass = RenderPass::Create();
  root_pass->SetNew(root_id, root_rect, root_rect, gfx::Transform());

  // Add a reference to the child surface on the root surface.
  SharedQuadState* root_shared_state =
      root_pass->CreateAndAppendSharedQuadState();
  root_shared_state->SetAll(gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                           0.0f, 1.0f, 0.0f, 50.0f,
                                           0.0f, 0.0f, 1.0f, 0.0f,
                                           0.0f, 0.0f, 0.0f, 1.0f),
                            root_size, root_rect, root_rect, false, 1.0f,
                            SkXfermode::kSrcOver_Mode, 0);
  SurfaceDrawQuad* surface_quad =
      root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(root_pass->shared_quad_state_list.back(), child_rect,
                       child_rect, child_surface_id);

  // Submit the root frame.
  scoped_ptr<DelegatedFrameData> root_delegated_frame_data(
      new DelegatedFrameData);
  root_delegated_frame_data->render_pass_list.push_back(root_pass.Pass());
  scoped_ptr<CompositorFrame> root_frame(new CompositorFrame);
  root_frame->delegated_frame_data = root_delegated_frame_data.Pass();
  factory.SubmitFrame(root_surface_id, root_frame.Pass(),
                      SurfaceFactory::DrawCallback());

  // Creates a child surface.
  factory.Create(child_surface_id);
  RenderPassId child_id(1, 1);
  scoped_ptr<RenderPass> child_pass = RenderPass::Create();
  child_pass->SetNew(child_id, child_rect, child_rect, gfx::Transform());

  // Add a solid quad in the child surface.
  SharedQuadState* child_shared_state =
      child_pass->CreateAndAppendSharedQuadState();
  child_shared_state->SetAll(gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                            0.0f, 1.0f, 0.0f, 50.0f,
                                            0.0f, 0.0f, 1.0f, 0.0f,
                                            0.0f, 0.0f, 0.0f, 1.0f),
                             root_size, root_rect, root_rect, false, 1.0f,
                             SkXfermode::kSrcOver_Mode, 0);
  SolidColorDrawQuad* color_quad =
      child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(child_pass->shared_quad_state_list.back(),
                     child_solid_quad_rect, child_solid_quad_rect,
                     SK_ColorYELLOW, false);

  // Submit the frame.
  scoped_ptr<DelegatedFrameData> child_delegated_frame_data(
      new DelegatedFrameData);
  child_delegated_frame_data->render_pass_list.push_back(child_pass.Pass());
  scoped_ptr<CompositorFrame> child_frame(new CompositorFrame);
  child_frame->delegated_frame_data = child_delegated_frame_data.Pass();
  factory.SubmitFrame(child_surface_id, child_frame.Pass(),
                      SurfaceFactory::DrawCallback());

  {
    SurfaceHittest hittest(&manager);
    gfx::Point transformed_point;
    EXPECT_EQ(root_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(10, 10),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(10, 10), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(99, 99),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(99, 99), transformed_point);
    EXPECT_EQ(child_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(100, 100),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(50, 50), transformed_point);
    EXPECT_EQ(child_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(199, 199),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(149, 149), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(200, 200),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(200, 200), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.Hittest(root_surface_id, gfx::Point(290, 290),
                              &transformed_point));
    EXPECT_EQ(gfx::Point(290, 290), transformed_point);
  }

  factory.Destroy(root_surface_id);
  factory.Destroy(child_surface_id);
}

}  // namespace cc
