// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include "base/test/test_simple_task_runner.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGraphicsMemoryAllocation.h"

using WebKit::WebGraphicsMemoryAllocation;

namespace cc {
namespace {

class TestOutputSurface : public OutputSurface {
 public:
  explicit TestOutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D> context3d)
      : OutputSurface(context3d.Pass()) {}

  explicit TestOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device)
      : OutputSurface(software_device.Pass()) {}

  TestOutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
                    scoped_ptr<cc::SoftwareOutputDevice> software_device)
      : OutputSurface(context3d.Pass(), software_device.Pass()) {}

  bool InitializeNewContext3D(
      scoped_ptr<WebKit::WebGraphicsContext3D> new_context3d) {
    return InitializeAndSetContext3D(new_context3d.Pass(),
                                     scoped_refptr<ContextProvider>());
  }

  using OutputSurface::ReleaseGL;

  bool HasClientForTesting() {
    return HasClient();
  }

  void OnVSyncParametersChangedForTesting(base::TimeTicks timebase,
                                          base::TimeDelta interval) {
    OnVSyncParametersChanged(timebase, interval);
  }

  void BeginFrameForTesting() {
    OutputSurface::BeginFrame(BeginFrameArgs::CreateExpiredForTesting());
  }

  void DidSwapBuffersForTesting() {
    DidSwapBuffers();
  }

  int pending_swap_buffers() {
    return pending_swap_buffers_;
  }

  void OnSwapBuffersCompleteForTesting() {
    OnSwapBuffersComplete(NULL);
  }

  void SetRetroactiveBeginFramePeriod(base::TimeDelta period) {
    retroactive_begin_frame_period_ = period;
  }

 protected:
  virtual void PostCheckForRetroactiveBeginFrame() OVERRIDE {
    // For testing purposes, we check immediately rather than posting a task.
    CheckForRetroactiveBeginFrame();
  }

  virtual base::TimeDelta RetroactiveBeginFramePeriod() OVERRIDE {
    return retroactive_begin_frame_period_;
  }

  base::TimeDelta retroactive_begin_frame_period_;
};

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientSuccess) {
  scoped_ptr<TestWebGraphicsContext3D> context3d =
      TestWebGraphicsContext3D::Create();

  TestOutputSurface output_surface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>());
  EXPECT_FALSE(output_surface.HasClientForTesting());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClientForTesting());
  EXPECT_FALSE(client.deferred_initialize_called());

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_output_surface_called());
  output_surface.context3d()->loseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  EXPECT_TRUE(client.did_lose_output_surface_called());
}

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientFailure) {
  scoped_ptr<TestWebGraphicsContext3D> context3d =
      TestWebGraphicsContext3D::Create();

  // Lose the context so BindToClient fails.
  context3d->set_times_make_current_succeeds(0);

  TestOutputSurface output_surface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>());
  EXPECT_FALSE(output_surface.HasClientForTesting());

  FakeOutputSurfaceClient client;
  EXPECT_FALSE(output_surface.BindToClient(&client));
  EXPECT_FALSE(output_surface.HasClientForTesting());
}

class InitializeNewContext3D : public ::testing::Test {
 public:
  InitializeNewContext3D()
      : context3d_(TestWebGraphicsContext3D::Create()),
        output_surface_(
            scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice)) {}

 protected:
  void BindOutputSurface() {
    EXPECT_TRUE(output_surface_.BindToClient(&client_));
    EXPECT_TRUE(output_surface_.HasClientForTesting());
  }

  void InitializeNewContextExpectFail() {
    EXPECT_FALSE(output_surface_.InitializeNewContext3D(
        context3d_.PassAs<WebKit::WebGraphicsContext3D>()));
    EXPECT_TRUE(output_surface_.HasClientForTesting());

    EXPECT_FALSE(output_surface_.context3d());
    EXPECT_TRUE(output_surface_.software_device());
  }

  scoped_ptr<TestWebGraphicsContext3D> context3d_;
  TestOutputSurface output_surface_;
  FakeOutputSurfaceClient client_;
};

TEST_F(InitializeNewContext3D, Success) {
  BindOutputSurface();
  EXPECT_FALSE(client_.deferred_initialize_called());

  EXPECT_TRUE(output_surface_.InitializeNewContext3D(
      context3d_.PassAs<WebKit::WebGraphicsContext3D>()));
  EXPECT_TRUE(client_.deferred_initialize_called());

  EXPECT_FALSE(client_.did_lose_output_surface_called());
  output_surface_.context3d()->loseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  EXPECT_TRUE(client_.did_lose_output_surface_called());

  output_surface_.ReleaseGL();
  EXPECT_FALSE(output_surface_.context3d());
}

TEST_F(InitializeNewContext3D, Context3dMakeCurrentFails) {
  BindOutputSurface();
  context3d_->set_times_make_current_succeeds(0);
  InitializeNewContextExpectFail();
}

TEST_F(InitializeNewContext3D, ClientDeferredInitializeFails) {
  BindOutputSurface();
  client_.set_deferred_initialize_result(false);
  InitializeNewContextExpectFail();
}

TEST(OutputSurfaceTest, BeginFrameEmulation) {
  scoped_ptr<TestWebGraphicsContext3D> context3d =
      TestWebGraphicsContext3D::Create();

  TestOutputSurface output_surface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>());
  EXPECT_FALSE(output_surface.HasClientForTesting());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClientForTesting());
  EXPECT_FALSE(client.deferred_initialize_called());

  // Initialize BeginFrame emulation
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  bool throttle_frame_production = true;
  const base::TimeDelta display_refresh_interval =
      BeginFrameArgs::DefaultInterval();

  output_surface.InitializeBeginFrameEmulation(
      task_runner.get(),
      throttle_frame_production,
      display_refresh_interval);

  output_surface.SetMaxFramesPending(2);
  output_surface.SetRetroactiveBeginFramePeriod(
      base::TimeDelta::FromSeconds(-1));

  // We should start off with 0 BeginFrames
  EXPECT_EQ(client.begin_frame_count(), 0);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // We should not have a pending task until a BeginFrame has been requested.
  EXPECT_FALSE(task_runner->HasPendingTask());
  output_surface.SetNeedsBeginFrame(true);
  EXPECT_TRUE(task_runner->HasPendingTask());

  // BeginFrame should be called on the first tick.
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_frame_count(), 1);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // BeginFrame should not be called when there is a pending BeginFrame.
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_frame_count(), 1);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // DidSwapBuffers should clear the pending BeginFrame.
  output_surface.DidSwapBuffersForTesting();
  EXPECT_EQ(client.begin_frame_count(), 1);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // BeginFrame should be throttled by pending swap buffers.
  output_surface.DidSwapBuffersForTesting();
  EXPECT_EQ(client.begin_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 2);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 2);

  // SwapAck should decrement pending swap buffers and unblock BeginFrame again.
  output_surface.OnSwapBuffersCompleteForTesting();
  EXPECT_EQ(client.begin_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_frame_count(), 3);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // Calling SetNeedsBeginFrame again indicates a swap did not occur but
  // the client still wants another BeginFrame.
  output_surface.SetNeedsBeginFrame(true);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_frame_count(), 4);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // Disabling SetNeedsBeginFrame should prevent further BeginFrames.
  output_surface.SetNeedsBeginFrame(false);
  task_runner->RunPendingTasks();
  EXPECT_FALSE(task_runner->HasPendingTask());
  EXPECT_EQ(client.begin_frame_count(), 4);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);
}

TEST(OutputSurfaceTest, OptimisticAndRetroactiveBeginFrames) {
  scoped_ptr<TestWebGraphicsContext3D> context3d =
      TestWebGraphicsContext3D::Create();

  TestOutputSurface output_surface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>());
  EXPECT_FALSE(output_surface.HasClientForTesting());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClientForTesting());
  EXPECT_FALSE(client.deferred_initialize_called());

  output_surface.SetMaxFramesPending(2);

  // Enable retroactive BeginFrames.
  output_surface.SetRetroactiveBeginFramePeriod(
    base::TimeDelta::FromSeconds(100000));

  // Optimistically injected BeginFrames should be throttled if
  // SetNeedsBeginFrame is false...
  output_surface.SetNeedsBeginFrame(false);
  output_surface.BeginFrameForTesting();
  EXPECT_EQ(client.begin_frame_count(), 0);
  // ...and retroactively triggered by a SetNeedsBeginFrame.
  output_surface.SetNeedsBeginFrame(true);
  EXPECT_EQ(client.begin_frame_count(), 1);

  // Optimistically injected BeginFrames should be throttled by pending
  // BeginFrames...
  output_surface.BeginFrameForTesting();
  EXPECT_EQ(client.begin_frame_count(), 1);
  // ...and retroactively triggered by a SetNeedsBeginFrame.
  output_surface.SetNeedsBeginFrame(true);
  EXPECT_EQ(client.begin_frame_count(), 2);
  // ...or retroactively triggered by a Swap.
  output_surface.BeginFrameForTesting();
  EXPECT_EQ(client.begin_frame_count(), 2);
  output_surface.DidSwapBuffersForTesting();
  EXPECT_EQ(client.begin_frame_count(), 3);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // Optimistically injected BeginFrames should be by throttled by pending
  // swap buffers...
  output_surface.DidSwapBuffersForTesting();
  EXPECT_EQ(client.begin_frame_count(), 3);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 2);
  output_surface.BeginFrameForTesting();
  EXPECT_EQ(client.begin_frame_count(), 3);
  // ...and retroactively triggered by OnSwapBuffersComplete
  output_surface.OnSwapBuffersCompleteForTesting();
  EXPECT_EQ(client.begin_frame_count(), 4);
}

TEST(OutputSurfaceTest, MemoryAllocation) {
  scoped_ptr<TestWebGraphicsContext3D> scoped_context =
      TestWebGraphicsContext3D::Create();
  TestWebGraphicsContext3D* context = scoped_context.get();

  TestOutputSurface output_surface(
      scoped_context.PassAs<WebKit::WebGraphicsContext3D>());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));

  WebGraphicsMemoryAllocation allocation;
  allocation.suggestHaveBackbuffer = true;
  allocation.bytesLimitWhenVisible = 1234;
  allocation.priorityCutoffWhenVisible =
      WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleOnly;
  allocation.bytesLimitWhenNotVisible = 4567;
  allocation.priorityCutoffWhenNotVisible =
      WebGraphicsMemoryAllocation::PriorityCutoffAllowNothing;

  context->SetMemoryAllocation(allocation);

  EXPECT_EQ(1234u, client.memory_policy().bytes_limit_when_visible);
  EXPECT_EQ(ManagedMemoryPolicy::CUTOFF_ALLOW_REQUIRED_ONLY,
            client.memory_policy().priority_cutoff_when_visible);
  EXPECT_EQ(4567u, client.memory_policy().bytes_limit_when_not_visible);
  EXPECT_EQ(ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING,
            client.memory_policy().priority_cutoff_when_not_visible);
  EXPECT_FALSE(client.discard_backbuffer_when_not_visible());

  allocation.suggestHaveBackbuffer = false;
  context->SetMemoryAllocation(allocation);
  EXPECT_TRUE(client.discard_backbuffer_when_not_visible());

  allocation.priorityCutoffWhenVisible =
      WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything;
  allocation.priorityCutoffWhenNotVisible =
      WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleAndNearby;
  context->SetMemoryAllocation(allocation);
  EXPECT_EQ(ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING,
            client.memory_policy().priority_cutoff_when_visible);
  EXPECT_EQ(ManagedMemoryPolicy::CUTOFF_ALLOW_NICE_TO_HAVE,
            client.memory_policy().priority_cutoff_when_not_visible);

  // 0 bytes limit should be ignored.
  allocation.bytesLimitWhenVisible = 0;
  context->SetMemoryAllocation(allocation);
  EXPECT_EQ(1234u, client.memory_policy().bytes_limit_when_visible);
}

}  // namespace
}  // namespace cc
