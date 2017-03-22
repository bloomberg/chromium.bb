// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
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

void RunPendingTasks(scoped_refptr<base::TaskRunner> task_runner) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(FROM_HERE,
                        Bind(&base::WaitableEvent::Signal, Unretained(&done)));
  done.Wait();
}

void DestroySurface(scoped_refptr<DirectCompositionSurfaceWin> surface) {
  scoped_refptr<base::TaskRunner> task_runner =
      surface->GetWindowTaskRunnerForTesting();
  DCHECK(surface->HasOneRef());

  surface = nullptr;

  // Ensure that the ChildWindowWin posts the task to delete the thread to the
  // main loop before doing RunUntilIdle. Otherwise the child threads could
  // outlive the main thread.
  RunPendingTasks(task_runner);

  base::RunLoop().RunUntilIdle();
}

TEST(DirectCompositionSurfaceTest, TestMakeCurrent) {
  if (!gl::QueryDirectCompositionDevice(
          gl::QueryD3D11DeviceObjectFromANGLE())) {
    LOG(WARNING)
        << "GL implementation not using DirectComposition, skipping test.";
    return;
  }

  TestImageTransportSurfaceDelegate delegate;

  scoped_refptr<DirectCompositionSurfaceWin> surface1(
      new DirectCompositionSurfaceWin(delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface1->Initialize());
  surface1->SetEnableDCLayers(true);

  scoped_refptr<gl::GLContext> context1 = gl::init::CreateGLContext(
      nullptr, surface1.get(), gl::GLContextAttribs());
  EXPECT_TRUE(surface1->Resize(gfx::Size(100, 100), 1.0, true));

  // First SetDrawRectangle must be full size of surface.
  EXPECT_FALSE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  // SetDrawRectangle can't be called again until swap.
  EXPECT_FALSE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_TRUE(context1->MakeCurrent(surface1.get()));
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface1->SwapBuffers());

  EXPECT_TRUE(context1->IsCurrent(surface1.get()));

  // SetDrawRectangle must be contained within surface.
  EXPECT_FALSE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 101, 101)));
  EXPECT_TRUE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context1->IsCurrent(surface1.get()));

  EXPECT_TRUE(surface1->Resize(gfx::Size(50, 50), 1.0, true));
  EXPECT_TRUE(context1->IsCurrent(surface1.get()));
  EXPECT_TRUE(surface1->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(context1->IsCurrent(surface1.get()));

  scoped_refptr<DirectCompositionSurfaceWin> surface2(
      new DirectCompositionSurfaceWin(delegate.AsWeakPtr(),
                                      ui::GetHiddenWindow()));
  EXPECT_TRUE(surface2->Initialize());

  scoped_refptr<gl::GLContext> context2 = gl::init::CreateGLContext(
      nullptr, surface2.get(), gl::GLContextAttribs());
  surface2->SetEnableDCLayers(true);
  EXPECT_TRUE(context2->MakeCurrent(surface2.get()));
  EXPECT_TRUE(surface2->Resize(gfx::Size(100, 100), 1.0, true));
  // The previous IDCompositionSurface should be suspended when another
  // surface is being drawn to.
  EXPECT_TRUE(surface2->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context2->IsCurrent(surface2.get()));

  // It should be possible to switch back to the previous surface and
  // unsuspend it.
  EXPECT_TRUE(context1->MakeCurrent(surface1.get()));
  context2 = nullptr;
  context1 = nullptr;

  DestroySurface(std::move(surface1));
  DestroySurface(std::move(surface2));
}

// Tests that switching using EnableDCLayers works.
TEST(DirectCompositionSurfaceTest, DXGIDCLayerSwitch) {
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
  EXPECT_FALSE(surface->swap_chain());

  // First SetDrawRectangle must be full size of surface for DXGI
  // swapchain.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface->swap_chain());

  // SetDrawRectangle can't be called again until swap.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_TRUE(context->MakeCurrent(surface.get()));
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());

  EXPECT_TRUE(context->IsCurrent(surface.get()));

  surface->SetEnableDCLayers(true);

  // Surface switched to use IDCompositionSurface, so must draw to
  // entire surface.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context->IsCurrent(surface.get()));
  EXPECT_FALSE(surface->swap_chain());

  surface->SetEnableDCLayers(false);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());

  // Surface switched to use IDXGISwapChain, so must draw to entire
  // surface.
  EXPECT_FALSE(surface->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface->swap_chain());

  context = nullptr;
  DestroySurface(std::move(surface));
}
}  // namespace
}  // namespace gpu
