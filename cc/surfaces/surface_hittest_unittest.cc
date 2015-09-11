// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
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

namespace {

class EmptySurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  void ReturnResources(const ReturnedResourceArray& resources) override {}
};

void CreateSharedQuadState(RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect) {
  SharedQuadState* child_shared_state =
      pass->CreateAndAppendSharedQuadState();
  child_shared_state->SetAll(transform,
                             root_rect.size(),
                             root_rect, root_rect, false, 1.0f,
                             SkXfermode::kSrcOver_Mode, 0);
}

void CreateSolidColorDrawQuad(RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect) {
  CreateSharedQuadState(pass, transform, root_rect);
  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(pass->shared_quad_state_list.back(),
                     quad_rect, quad_rect,
                     SK_ColorYELLOW, false);
}

void CreateRenderPassDrawQuad(RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect,
                              const RenderPassId& render_pass_id) {
  CreateSharedQuadState(pass, transform, root_rect);
  RenderPassDrawQuad* render_pass_quad =
      pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(pass->shared_quad_state_list.back(),
                           quad_rect, quad_rect,
                           render_pass_id,
                           ResourceId(),
                           gfx::Vector2dF(),
                           gfx::Size(),
                           FilterOperations(),
                           gfx::Vector2dF(),
                           FilterOperations());
}
void CreateSurfaceDrawQuad(RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect,
                           const gfx::Rect& quad_rect,
                           SurfaceId surface_id) {
  CreateSharedQuadState(pass, transform, root_rect);
  SurfaceDrawQuad* surface_quad =
      pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(pass->shared_quad_state_list.back(),
                       quad_rect, quad_rect,
                       surface_id);
}

scoped_ptr<CompositorFrame> CreateCompositorFrame(
    const gfx::Rect& root_rect,
    RenderPass** render_pass) {
  RenderPassId root_id(1, 1);
  scoped_ptr<RenderPass> root_pass = RenderPass::Create();
  root_pass->SetNew(root_id, root_rect, root_rect, gfx::Transform());

  scoped_ptr<DelegatedFrameData> root_delegated_frame_data(
      new DelegatedFrameData);
  root_delegated_frame_data->render_pass_list.push_back(root_pass.Pass());
  scoped_ptr<CompositorFrame> root_frame(new CompositorFrame);
  root_frame->delegated_frame_data = root_delegated_frame_data.Pass();

  *render_pass = root_frame->delegated_frame_data->render_pass_list.back();
  return root_frame.Pass();
}

}  // namespace

// This test verifies that hit testing on a surface that does not exist does
// not crash.
TEST(SurfaceHittestTest, Hittest_BadCompositorFrameDoesNotCrash) {
  SurfaceManager manager;
  EmptySurfaceFactoryClient client;
  SurfaceFactory factory(&manager, &client);

  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  scoped_ptr<CompositorFrame> root_frame =
      CreateCompositorFrame(root_rect, &root_pass);

  // Add a reference to a non-existant child surface on the root surface.
  SurfaceIdAllocator child_allocator(3);
  SurfaceId child_surface_id;
  child_surface_id.id = 0xdeadbeef;
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(),
                        root_rect,
                        child_rect,
                        child_surface_id);

  // Submit the root frame.
  SurfaceIdAllocator root_allocator(2);
  SurfaceId root_surface_id = root_allocator.GenerateId();
  factory.Create(root_surface_id);
  factory.SubmitCompositorFrame(root_surface_id, root_frame.Pass(),
                                SurfaceFactory::DrawCallback());

  {
    SurfaceHittest hittest(&manager);
    // It is expected this test will complete without crashes.
    gfx::Point transformed_point;
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(100, 100), &transformed_point));
  }

  factory.Destroy(root_surface_id);
}

TEST(SurfaceHittestTest, Hittest_SingleSurface) {
  SurfaceManager manager;
  EmptySurfaceFactoryClient client;
  SurfaceFactory factory(&manager, &client);

  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  scoped_ptr<CompositorFrame> root_frame =
      CreateCompositorFrame(root_rect, &root_pass);

  // Submit the root frame.
  SurfaceIdAllocator root_allocator(2);
  SurfaceId root_surface_id = root_allocator.GenerateId();
  factory.Create(root_surface_id);
  factory.SubmitCompositorFrame(root_surface_id, root_frame.Pass(),
                                SurfaceFactory::DrawCallback());

  {
    SurfaceHittest hittest(&manager);
    gfx::Point transformed_point;
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(100, 100), &transformed_point));
    EXPECT_EQ(gfx::Point(100, 100), transformed_point);
  }

  factory.Destroy(root_surface_id);
}

TEST(SurfaceHittestTest, Hittest_ChildSurface) {
  SurfaceManager manager;
  EmptySurfaceFactoryClient client;
  SurfaceFactory factory(&manager, &client);

  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  scoped_ptr<CompositorFrame> root_frame =
      CreateCompositorFrame(root_rect, &root_pass);

  // Add a reference to the child surface on the root surface.
  SurfaceIdAllocator child_allocator(3);
  SurfaceId child_surface_id = child_allocator.GenerateId();
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                       0.0f, 1.0f, 0.0f, 50.0f,
                                       0.0f, 0.0f, 1.0f, 0.0f,
                                       0.0f, 0.0f, 0.0f, 1.0f),
                        root_rect,
                        child_rect,
                        child_surface_id);

  // Submit the root frame.
  SurfaceIdAllocator root_allocator(2);
  SurfaceId root_surface_id = root_allocator.GenerateId();
  factory.Create(root_surface_id);
  factory.SubmitCompositorFrame(root_surface_id, root_frame.Pass(),
                                SurfaceFactory::DrawCallback());

  // Creates a child surface.
  RenderPass* child_pass = nullptr;
  scoped_ptr<CompositorFrame> child_frame =
      CreateCompositorFrame(child_rect, &child_pass);

  // Add a solid quad in the child surface.
  gfx::Rect child_solid_quad_rect(100, 100);
  CreateSolidColorDrawQuad(child_pass,
                           gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                         0.0f, 1.0f, 0.0f, 50.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f),
                           root_rect,
                           child_solid_quad_rect);

  // Submit the frame.
  factory.Create(child_surface_id);
  factory.SubmitCompositorFrame(child_surface_id, child_frame.Pass(),
                                SurfaceFactory::DrawCallback());

  {
    SurfaceHittest hittest(&manager);
    gfx::Point transformed_point;
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(10, 10), &transformed_point));
    EXPECT_EQ(gfx::Point(10, 10), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(99, 99), &transformed_point));
    EXPECT_EQ(gfx::Point(99, 99), transformed_point);
    EXPECT_EQ(child_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(100, 100), &transformed_point));
    EXPECT_EQ(gfx::Point(50, 50), transformed_point);
    EXPECT_EQ(child_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(199, 199), &transformed_point));
    EXPECT_EQ(gfx::Point(149, 149), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(200, 200), &transformed_point));
    EXPECT_EQ(gfx::Point(200, 200), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(290, 290), &transformed_point));
    EXPECT_EQ(gfx::Point(290, 290), transformed_point);
  }

  factory.Destroy(root_surface_id);
  factory.Destroy(child_surface_id);
}

// This test verifies that hit testing will progress to the next quad if it
// encounters an invalid RenderPassDrawQuad for whatever reason.
TEST(SurfaceHittestTest, Hittest_InvalidRenderPassDrawQuad) {
  SurfaceManager manager;
  EmptySurfaceFactoryClient client;
  SurfaceFactory factory(&manager, &client);

  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  scoped_ptr<CompositorFrame> root_frame =
      CreateCompositorFrame(root_rect, &root_pass);

  // Create a RenderPassDrawQuad to a non-existant RenderPass.
  CreateRenderPassDrawQuad(root_pass,
                           gfx::Transform(),
                           root_rect,
                           root_rect,
                           RenderPassId(1337, 1337));

  // Add a reference to the child surface on the root surface.
  SurfaceIdAllocator child_allocator(3);
  SurfaceId child_surface_id = child_allocator.GenerateId();
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                       0.0f, 1.0f, 0.0f, 50.0f,
                                       0.0f, 0.0f, 1.0f, 0.0f,
                                       0.0f, 0.0f, 0.0f, 1.0f),
                        root_rect,
                        child_rect,
                        child_surface_id);

  // Submit the root frame.
  SurfaceIdAllocator root_allocator(2);
  SurfaceId root_surface_id = root_allocator.GenerateId();
  factory.Create(root_surface_id);
  factory.SubmitCompositorFrame(root_surface_id, root_frame.Pass(),
                                SurfaceFactory::DrawCallback());

  // Creates a child surface.
  RenderPass* child_pass = nullptr;
  scoped_ptr<CompositorFrame> child_frame =
      CreateCompositorFrame(child_rect, &child_pass);

  // Add a solid quad in the child surface.
  gfx::Rect child_solid_quad_rect(100, 100);
  CreateSolidColorDrawQuad(child_pass,
                           gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                         0.0f, 1.0f, 0.0f, 50.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f),
                           root_rect,
                           child_solid_quad_rect);

  // Submit the frame.
  factory.Create(child_surface_id);
  factory.SubmitCompositorFrame(child_surface_id, child_frame.Pass(),
                                SurfaceFactory::DrawCallback());

  {
    SurfaceHittest hittest(&manager);
    gfx::Point transformed_point;
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(10, 10), &transformed_point));
    EXPECT_EQ(gfx::Point(10, 10), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(99, 99), &transformed_point));
    EXPECT_EQ(gfx::Point(99, 99), transformed_point);
    EXPECT_EQ(child_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(100, 100), &transformed_point));
    EXPECT_EQ(gfx::Point(50, 50), transformed_point);
    EXPECT_EQ(child_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(199, 199), &transformed_point));
    EXPECT_EQ(gfx::Point(149, 149), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(200, 200), &transformed_point));
    EXPECT_EQ(gfx::Point(200, 200), transformed_point);
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(290, 290), &transformed_point));
    EXPECT_EQ(gfx::Point(290, 290), transformed_point);
  }

  factory.Destroy(root_surface_id);
  factory.Destroy(child_surface_id);
}

}  // namespace cc
