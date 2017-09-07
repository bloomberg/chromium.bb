// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/env.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

class SurfaceTest : public test::ExoTestBase,
                    public ::testing::WithParamInterface<float> {
 public:
  SurfaceTest() = default;
  ~SurfaceTest() override = default;
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Set the device scale factor.
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", device_scale_factor()));
    test::ExoTestBase::SetUp();
  }

  void TearDown() override {
    test::ExoTestBase::TearDown();
    display::Display::ResetForceDeviceScaleFactorForTesting();
  }

  float device_scale_factor() const { return GetParam(); }

  gfx::Rect ToPixel(const gfx::Rect rect) {
    return gfx::ConvertRectToPixel(device_scale_factor(), rect);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceTest);
};

void ReleaseBuffer(int* release_buffer_call_count) {
  (*release_buffer_call_count)++;
}

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(, SurfaceTest, testing::Values(1.0f, 1.25f, 2.0f));

TEST_P(SurfaceTest, Attach) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  // Set the release callback that will be run when buffer is no longer in use.
  int release_buffer_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&ReleaseBuffer, base::Unretained(&release_buffer_call_count)));

  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to surface1.
  surface->Attach(buffer.get());
  surface->Commit();

  // Commit without calling Attach() should have no effect.
  surface->Commit();
  EXPECT_EQ(0, release_buffer_call_count);

  // Attach a null buffer to surface, this should release the previously
  // attached buffer.
  surface->Attach(nullptr);
  surface->Commit();
  // LayerTreeFrameSinkHolder::ReclaimResources() gets called via
  // CompositorFrameSinkClient interface. We need to wait here for the mojo
  // call to finish so that the release callback finishes running before
  // the assertion below.
  RunAllPendingInMessageLoop();
  ASSERT_EQ(1, release_buffer_call_count);
}

TEST_P(SurfaceTest, Damage) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to the surface. This will update the pending bounds of
  // the surface to the buffer size.
  surface->Attach(buffer.get());

  // Mark areas inside the bounds of the surface as damaged. This should result
  // in pending damage.
  surface->Damage(gfx::Rect(0, 0, 10, 10));
  surface->Damage(gfx::Rect(10, 10, 10, 10));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(0, 0, 10, 10)));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(10, 10, 10, 10)));
  EXPECT_FALSE(surface->HasPendingDamageForTesting(gfx::Rect(5, 5, 10, 10)));

  // Check that damage larger than contents is handled correctly at commit.
  surface->Damage(gfx::Rect(gfx::ScaleToCeiledSize(buffer_size, 2.0f)));
  surface->Commit();
}

void SetFrameTime(base::TimeTicks* result, base::TimeTicks frame_time) {
  *result = frame_time;
}

TEST_P(SurfaceTest, RequestFrameCallback) {
  std::unique_ptr<Surface> surface(new Surface);

  base::TimeTicks frame_time;
  surface->RequestFrameCallback(
      base::Bind(&SetFrameTime, base::Unretained(&frame_time)));
  surface->Commit();

  // Callback should not run synchronously.
  EXPECT_TRUE(frame_time.is_null());
}

const cc::CompositorFrame& GetFrameFromSurface(ShellSurface* shell_surface) {
  viz::SurfaceId surface_id = shell_surface->host_window()->GetSurfaceId();
  viz::SurfaceManager* surface_manager = aura::Env::GetInstance()
                                             ->context_factory_private()
                                             ->GetFrameSinkManager()
                                             ->surface_manager();
  const cc::CompositorFrame& frame =
      surface_manager->GetSurfaceForId(surface_id)->GetActiveFrame();
  return frame;
}

TEST_P(SurfaceTest, SetOpaqueRegion) {
  gfx::Size buffer_size(1, 1);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  // Attaching a buffer with alpha channel.
  surface->Attach(buffer.get());

  // Setting an opaque region that contains the buffer size doesn't require
  // draw with blending.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeWH(256, 256)));
  surface->Commit();
  RunAllPendingInMessageLoop();

  {
    const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_FALSE(frame.render_pass_list.back()
                     ->quad_list.back()
                     ->ShouldDrawWithBlending());
    EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 1, 1)),
              frame.render_pass_list.back()->damage_rect);
  }

  // Setting an empty opaque region requires draw with blending.
  surface->SetOpaqueRegion(SkRegion(SkIRect::MakeEmpty()));
  surface->Commit();
  RunAllPendingInMessageLoop();

  {
    const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_TRUE(frame.render_pass_list.back()
                    ->quad_list.back()
                    ->ShouldDrawWithBlending());
    EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 1, 1)),
              frame.render_pass_list.back()->damage_rect);
  }

  std::unique_ptr<Buffer> buffer_without_alpha(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(
          buffer_size, gfx::BufferFormat::RGBX_8888)));

  // Attaching a buffer without an alpha channel doesn't require draw with
  // blending.
  surface->Attach(buffer_without_alpha.get());
  surface->Commit();
  RunAllPendingInMessageLoop();

  {
    const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_FALSE(frame.render_pass_list.back()
                     ->quad_list.back()
                     ->ShouldDrawWithBlending());
    EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 0, 0)),
              frame.render_pass_list.back()->damage_rect);
  }
}

TEST_P(SurfaceTest, SetInputRegion) {
  std::unique_ptr<Surface> surface(new Surface);

  // Setting a non-empty input region should succeed.
  surface->SetInputRegion(SkRegion(SkIRect::MakeWH(256, 256)));

  // Setting an empty input region should succeed.
  surface->SetInputRegion(SkRegion(SkIRect::MakeEmpty()));
}

TEST_P(SurfaceTest, SetBufferScale) {
  gfx::Size buffer_size(512, 512);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the buffer scale into
  // account.
  const float kBufferScale = 2.0f;
  surface->Attach(buffer.get());
  surface->SetBufferScale(kBufferScale);
  surface->Commit();
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->window()->bounds().size().ToString());
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->content_size().ToString());

  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 256, 256)),
            frame.render_pass_list.back()->damage_rect);
}

TEST_P(SurfaceTest, SetBufferTransform) {
  gfx::Size buffer_size(256, 512);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the buffer transform
  // into account.
  surface->Attach(buffer.get());
  surface->SetBufferTransform(Transform::ROTATE_90);
  surface->Commit();
  EXPECT_EQ(gfx::Size(buffer_size.height(), buffer_size.width()),
            surface->window()->bounds().size());
  EXPECT_EQ(gfx::Size(buffer_size.height(), buffer_size.width()),
            surface->content_size());

  RunAllPendingInMessageLoop();

  {
    const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, buffer_size.height(), buffer_size.width())),
        frame.render_pass_list.back()->damage_rect);
    const cc::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, 512, 256)),
        cc::MathUtil::MapEnclosingClippedRect(
            quad_list.front()->shared_quad_state->quad_to_target_transform,
            quad_list.front()->rect));
  }

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  auto child_surface = base::MakeUnique<Surface>();
  auto sub_surface =
      base::MakeUnique<SubSurface>(child_surface.get(), surface.get());

  // Set position to 20, 10.
  gfx::Point child_position(20, 10);
  sub_surface->SetPosition(child_position);

  child_surface->Attach(child_buffer.get());
  child_surface->SetBufferTransform(Transform::ROTATE_180);
  const int kChildBufferScale = 2;
  child_surface->SetBufferScale(kChildBufferScale);
  child_surface->Commit();
  surface->Commit();
  EXPECT_EQ(
      gfx::ScaleToRoundedSize(child_buffer_size, 1.0f / kChildBufferScale),
      child_surface->window()->bounds().size());
  EXPECT_EQ(
      gfx::ScaleToRoundedSize(child_buffer_size, 1.0f / kChildBufferScale),
      child_surface->content_size());

  RunAllPendingInMessageLoop();

  {
    const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const cc::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(2u, quad_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(child_position,
                          gfx::ScaleToRoundedSize(child_buffer_size,
                                                  1.0f / kChildBufferScale))),
        cc::MathUtil::MapEnclosingClippedRect(
            quad_list.front()->shared_quad_state->quad_to_target_transform,
            quad_list.front()->rect));
  }
}

TEST_P(SurfaceTest, MirrorLayers) {
  gfx::Size buffer_size(512, 512);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_EQ(buffer_size, surface->window()->bounds().size());
  EXPECT_EQ(buffer_size, surface->window()->layer()->bounds().size());
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::MirrorLayers(shell_surface->host_window(), false /* sync_bounds */);
  EXPECT_EQ(buffer_size, surface->window()->bounds().size());
  EXPECT_EQ(buffer_size, surface->window()->layer()->bounds().size());
  EXPECT_EQ(buffer_size, old_layer_owner->root()->bounds().size());
  EXPECT_TRUE(shell_surface->host_window()->layer()->has_external_content());
  EXPECT_TRUE(old_layer_owner->root()->has_external_content());
}

TEST_P(SurfaceTest, SetViewport) {
  gfx::Size buffer_size(1, 1);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the viewport into
  // account.
  surface->Attach(buffer.get());
  gfx::Size viewport(256, 256);
  surface->SetViewport(viewport);
  surface->Commit();
  EXPECT_EQ(viewport.ToString(), surface->content_size().ToString());

  // This will update the bounds of the surface and take the viewport2 into
  // account.
  gfx::Size viewport2(512, 512);
  surface->SetViewport(viewport2);
  surface->Commit();
  EXPECT_EQ(viewport2.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(viewport2.ToString(), surface->content_size().ToString());

  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 512, 512)),
            frame.render_pass_list.back()->damage_rect);
}

TEST_P(SurfaceTest, SetCrop) {
  gfx::Size buffer_size(16, 16);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  gfx::Size crop_size(12, 12);
  surface->SetCrop(gfx::RectF(gfx::PointF(2.0, 2.0), gfx::SizeF(crop_size)));
  surface->Commit();
  EXPECT_EQ(crop_size.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(crop_size.ToString(), surface->content_size().ToString());

  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 12, 12)),
            frame.render_pass_list.back()->damage_rect);
}

TEST_P(SurfaceTest, SetBlendMode) {
  gfx::Size buffer_size(1, 1);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->SetBlendMode(SkBlendMode::kSrc);
  surface->Commit();
  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
  EXPECT_FALSE(frame.render_pass_list.back()
                   ->quad_list.back()
                   ->ShouldDrawWithBlending());
}

TEST_P(SurfaceTest, OverlayCandidate) {
  gfx::Size buffer_size(1, 1);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true);
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
  viz::DrawQuad* draw_quad = frame.render_pass_list.back()->quad_list.back();
  ASSERT_EQ(viz::DrawQuad::TEXTURE_CONTENT, draw_quad->material);

  const cc::TextureDrawQuad* texture_quad =
      cc::TextureDrawQuad::MaterialCast(draw_quad);
  EXPECT_FALSE(texture_quad->resource_size_in_pixels().IsEmpty());
}

TEST_P(SurfaceTest, SetAlpha) {
  gfx::Size buffer_size(1, 1);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true);
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->SetAlpha(0.5f);
  surface->Commit();
  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 1, 1)),
            frame.render_pass_list.back()->damage_rect);
}

TEST_P(SurfaceTest, Commit) {
  std::unique_ptr<Surface> surface(new Surface);

  // Calling commit without a buffer should succeed.
  surface->Commit();
}

TEST_P(SurfaceTest, SendsBeginFrameAcks) {
  viz::FakeExternalBeginFrameSource source(0.f, false);
  gfx::Size buffer_size(1, 1);
  auto buffer = base::MakeUnique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true);
  auto surface = base::MakeUnique<Surface>();
  auto shell_surface = base::MakeUnique<ShellSurface>(surface.get());
  shell_surface->SetBeginFrameSource(&source);
  surface->Attach(buffer.get());

  // Request a frame callback so that Surface now needs BeginFrames.
  base::TimeTicks frame_time;
  surface->RequestFrameCallback(
      base::Bind(&SetFrameTime, base::Unretained(&frame_time)));
  surface->Commit();  // Move callback from pending callbacks to current ones.
  RunAllPendingInMessageLoop();

  // Surface should add itself as observer during
  // DidReceiveCompositorFrameAck().
  shell_surface->DidReceiveCompositorFrameAck();
  EXPECT_EQ(1u, source.num_observers());

  viz::BeginFrameArgs args(source.CreateBeginFrameArgs(BEGINFRAME_FROM_HERE));
  args.frame_time = base::TimeTicks::FromInternalValue(100);
  source.TestOnBeginFrame(args);  // Runs the frame callback.
  EXPECT_EQ(args.frame_time, frame_time);

  surface->Commit();  // Acknowledges the BeginFrame via CompositorFrame.
  RunAllPendingInMessageLoop();

  const cc::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  viz::BeginFrameAck expected_ack(args.source_id, args.sequence_number, true);
  EXPECT_EQ(expected_ack, frame.metadata.begin_frame_ack);

  // TODO(eseckler): Add test for DidNotProduceFrame plumbing.
}

}  // namespace
}  // namespace exo
