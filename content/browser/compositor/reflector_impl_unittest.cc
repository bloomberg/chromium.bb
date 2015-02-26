// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/context_factories_for_test.h"

namespace content {
namespace {
class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  FakeTaskRunner() {}

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    return true;
  }
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return true;
  }
  bool RunsTasksOnCurrentThread() const override { return true; }

 protected:
  ~FakeTaskRunner() override {}
};

class TestOutputSurface : public BrowserCompositorOutputSurface {
 public:
  TestOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager)
      : BrowserCompositorOutputSurface(context_provider,
                                       vsync_manager) {}

  void SetFlip(bool flip) { capabilities_.flipped_output_surface = flip; }

  void SwapBuffers(cc::CompositorFrame* frame) override {}

#if defined(OS_MACOSX)
  void OnSurfaceDisplayed() override {}
  void OnSurfaceRecycled() override {}
  bool ShouldNotShowFramesAfterRecycle() const override { return false; }
#endif

  gfx::Size SurfaceSize() const override { return gfx::Size(256, 256); }
};

const gfx::Rect kSubRect = gfx::Rect(0, 0, 64, 64);
const SkIRect kSkSubRect = SkIRect::MakeXYWH(0, 0, 64, 64);

}  // namespace

class ReflectorImplTest : public testing::Test {
 public:
  void SetUp() override {
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);
    ImageTransportFactory::InitializeForUnitTests(
        scoped_ptr<ImageTransportFactory>(
            new NoTransportImageTransportFactory));
    message_loop_.reset(new base::MessageLoop());
    proxy_ = message_loop_->message_loop_proxy();
    compositor_task_runner_ = new FakeTaskRunner();
    compositor_.reset(new ui::Compositor(gfx::kNullAcceleratedWidget,
                                         context_factory,
                                         compositor_task_runner_.get()));
    context_provider_ = cc::TestContextProvider::Create(
        cc::TestWebGraphicsContext3D::Create().Pass());
    output_surface_ =
        scoped_ptr<TestOutputSurface>(
            new TestOutputSurface(context_provider_,
                                  compositor_->vsync_manager())).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    mirroring_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    gfx::Size size = output_surface_->SurfaceSize();
    mirroring_layer_->SetBounds(gfx::Rect(size.width(), size.height()));
  }

  void SetUpReflector() {
    reflector_ = make_scoped_ptr(
        new ReflectorImpl(compositor_.get(), mirroring_layer_.get()));
    reflector_->OnSourceSurfaceReady(output_surface_.get());
  }

  void TearDown() override {
    cc::TextureMailbox mailbox;
    scoped_ptr<cc::SingleReleaseCallback> release;
    if (mirroring_layer_->PrepareTextureMailbox(&mailbox, &release, false)) {
      release->Run(0, false);
    }
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
    ImageTransportFactory::Terminate();
  }

  void UpdateTexture() { reflector_->OnSourcePostSubBuffer(kSubRect); }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<base::MessageLoopProxy> proxy_;
  scoped_ptr<ui::Compositor> compositor_;
  scoped_ptr<ui::Layer> mirroring_layer_;
  scoped_ptr<ReflectorImpl> reflector_;
  scoped_ptr<TestOutputSurface> output_surface_;
};

namespace {
TEST_F(ReflectorImplTest, CheckNormalOutputSurface) {
  output_surface_->SetFlip(false);
  SetUpReflector();
  UpdateTexture();
  EXPECT_TRUE(mirroring_layer_->TextureFlipped());
  EXPECT_EQ(SkRegion(SkIRect::MakeXYWH(
                0, output_surface_->SurfaceSize().height() - kSubRect.height(),
                kSubRect.width(), kSubRect.height())),
            mirroring_layer_->damaged_region());
}

TEST_F(ReflectorImplTest, CheckInvertedOutputSurface) {
  output_surface_->SetFlip(true);
  SetUpReflector();
  UpdateTexture();
  EXPECT_FALSE(mirroring_layer_->TextureFlipped());
  EXPECT_EQ(SkRegion(kSkSubRect), mirroring_layer_->damaged_region());
}

}  // namespace
}  // namespace content
