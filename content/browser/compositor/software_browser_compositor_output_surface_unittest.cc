// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/fake_output_surface_client.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/vsync_provider.h"

namespace {

class FakeVSyncProvider : public gfx::VSyncProvider {
 public:
  FakeVSyncProvider() : call_count_(0) {}
  ~FakeVSyncProvider() override {}

  void GetVSyncParameters(UpdateVSyncCallback callback) override {
    std::move(callback).Run(timebase_, interval_);
    call_count_++;
  }

  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    return false;
  }

  bool SupportGetVSyncParametersIfAvailable() const override { return false; }
  bool IsHWClock() const override { return false; }

  int call_count() const { return call_count_; }

  void set_timebase(base::TimeTicks timebase) { timebase_ = timebase; }
  void set_interval(base::TimeDelta interval) { interval_ = interval; }

 private:
  base::TimeTicks timebase_;
  base::TimeDelta interval_;

  int call_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeVSyncProvider);
};

class FakeSoftwareOutputDevice : public viz::SoftwareOutputDevice {
 public:
  FakeSoftwareOutputDevice() : vsync_provider_(new FakeVSyncProvider()) {}
  ~FakeSoftwareOutputDevice() override {}

  gfx::VSyncProvider* GetVSyncProvider() override {
    return vsync_provider_.get();
  }

 private:
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeSoftwareOutputDevice);
};

}  // namespace

class SoftwareBrowserCompositorOutputSurfaceTest : public testing::Test {
 public:
  SoftwareBrowserCompositorOutputSurfaceTest()
      : begin_frame_source_(std::make_unique<viz::DelayBasedTimeSource>(
                                message_loop_.task_runner().get()),
                            viz::BeginFrameSource::kNotRestartableId) {}
  ~SoftwareBrowserCompositorOutputSurfaceTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);

  std::unique_ptr<content::BrowserCompositorOutputSurface> CreateSurface(
      std::unique_ptr<viz::SoftwareOutputDevice> device);

 protected:
  std::unique_ptr<content::BrowserCompositorOutputSurface> output_surface_;

  // TODO(crbug.com/616973): We shouldn't be using ThreadTaskRunnerHandle::Get()
  // inside the OutputSurface, so we shouldn't need a MessageLoop. The
  // OutputSurface should be using the TaskRunner given to the compositor.
  base::TestMessageLoop message_loop_;
  viz::DelayBasedBeginFrameSource begin_frame_source_;
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<ui::Compositor> compositor_;
  int update_vsync_parameters_call_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareBrowserCompositorOutputSurfaceTest);
};

void SoftwareBrowserCompositorOutputSurfaceTest::SetUp() {
  const bool enable_pixel_output = false;
  context_factories_ =
      std::make_unique<ui::TestContextFactories>(enable_pixel_output);

  compositor_ = std::make_unique<ui::Compositor>(
      context_factories_->GetContextFactoryPrivate()->AllocateFrameSinkId(),
      context_factories_->GetContextFactory(),
      context_factories_->GetContextFactoryPrivate(),
      message_loop_.task_runner().get(), false /* enable_pixel_canvas */);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

void SoftwareBrowserCompositorOutputSurfaceTest::TearDown() {
  output_surface_.reset();
  compositor_.reset();
  context_factories_.reset();
}

std::unique_ptr<content::BrowserCompositorOutputSurface>
SoftwareBrowserCompositorOutputSurfaceTest::CreateSurface(
    std::unique_ptr<viz::SoftwareOutputDevice> device) {
  auto surface =
      std::make_unique<content::SoftwareBrowserCompositorOutputSurface>(
          std::move(device));
  surface->SetUpdateVSyncParametersCallback(base::BindRepeating(
      &SoftwareBrowserCompositorOutputSurfaceTest::UpdateVSyncParameters,
      base::Unretained(this)));
  return surface;
}

void SoftwareBrowserCompositorOutputSurfaceTest::UpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  update_vsync_parameters_call_count_++;
}

TEST_F(SoftwareBrowserCompositorOutputSurfaceTest, NoVSyncProvider) {
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<viz::SoftwareOutputDevice> software_device(
      new viz::SoftwareOutputDevice());
  output_surface_ = CreateSurface(std::move(software_device));
  output_surface_->BindToClient(&output_surface_client);

  output_surface_->SwapBuffers(viz::OutputSurfaceFrame());
  EXPECT_EQ(nullptr, output_surface_->software_device()->GetVSyncProvider());
  EXPECT_EQ(0, update_vsync_parameters_call_count_);
}

TEST_F(SoftwareBrowserCompositorOutputSurfaceTest, VSyncProviderUpdates) {
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<viz::SoftwareOutputDevice> software_device(
      new FakeSoftwareOutputDevice());
  output_surface_ = CreateSurface(std::move(software_device));
  output_surface_->BindToClient(&output_surface_client);

  FakeVSyncProvider* vsync_provider = static_cast<FakeVSyncProvider*>(
      output_surface_->software_device()->GetVSyncProvider());
  EXPECT_EQ(0, vsync_provider->call_count());

  output_surface_->SwapBuffers(viz::OutputSurfaceFrame());
  EXPECT_EQ(1, vsync_provider->call_count());
  EXPECT_EQ(1, update_vsync_parameters_call_count_);
}
