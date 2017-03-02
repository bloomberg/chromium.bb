// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class TestImageTransportSurfaceDelegate
    : public ImageTransportSurfaceDelegate,
      public base::SupportsWeakPtr<TestImageTransportSurfaceDelegate> {
 public:
  ~TestImageTransportSurfaceDelegate() override {}

  // ImageTransportSurfaceDelegate implementation.
  void DidCreateAcceleratedSurfaceChildWindow(
      SurfaceHandle parent_window,
      SurfaceHandle child_window) override {}
  void DidSwapBuffersComplete(SwapBuffersCompleteParams params) override {}
  const gles2::FeatureInfo* GetFeatureInfo() const override { return nullptr; }
  void SetLatencyInfoCallback(const LatencyInfoCallback& callback) override {}
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override {}
  void AddFilter(IPC::MessageFilter* message_filter) override {}
  int32_t GetRouteID() const override { return 0; }
};

TEST(DirectCompositionSurfaceTest, TestMakeCurrent) {
  if (!gl::QueryDirectCompositionDevice(
          gl::QueryD3D11DeviceObjectFromANGLE())) {
    LOG(WARNING)
        << "GL implementation not using DirectComposition, skipping test.";
    return;
  }

  TestImageTransportSurfaceDelegate delegate;

  scoped_refptr<DirectCompositionSurfaceWin> surface(
      new DirectCompositionSurfaceWin(delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface->Initialize());

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  EXPECT_TRUE(surface->Resize(gfx::Size(100, 100), 1.0, true));

  // First SetDrawRectangle must be full size of surface.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  // SetDrawRectangle can't be called again until swap.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_TRUE(context->MakeCurrent(surface.get()));
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());

  EXPECT_TRUE(context->IsCurrent(surface.get()));

  // SetDrawRectangle must be contained within surface.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 101, 101)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context->IsCurrent(surface.get()));

  EXPECT_TRUE(surface->Resize(gfx::Size(50, 50), 1.0, true));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(context->IsCurrent(surface.get()));

  scoped_refptr<DirectCompositionSurfaceWin> surface2(
      new DirectCompositionSurfaceWin(delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface2->Initialize());

  scoped_refptr<gl::GLContext> context2 = gl::init::CreateGLContext(
      nullptr, surface2.get(), gl::GLContextAttribs());
  EXPECT_TRUE(context2->MakeCurrent(surface2.get()));
  EXPECT_TRUE(surface2->Resize(gfx::Size(100, 100), 1.0, true));
  // The previous IDCompositionSurface should be suspended when another
  // surface is being drawn to.
  EXPECT_TRUE(surface2->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context2->IsCurrent(surface2.get()));

  // It should be possible to switch back to the previous surface and
  // unsuspend it.
  EXPECT_TRUE(context->MakeCurrent(surface.get()));

  context2 = nullptr;
  surface2 = nullptr;
  context = nullptr;
  surface = nullptr;
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace gpu
