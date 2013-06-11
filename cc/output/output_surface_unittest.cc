// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  OutputSurfaceClient* client() { return client_; }

  bool InitializeNewContext3D(
      scoped_ptr<WebKit::WebGraphicsContext3D> new_context3d) {
    return InitializeAndSetContext3D(new_context3d.Pass(),
                                     scoped_refptr<ContextProvider>());
  }
};

class FakeOutputSurfaceClient : public OutputSurfaceClient {
 public:
  FakeOutputSurfaceClient()
      : deferred_initialize_result_(true),
        deferred_initialize_called_(false),
        did_lose_output_surface_called_(false) {}

  virtual bool DeferredInitialize(
      scoped_refptr<ContextProvider> offscreen_context_provider) OVERRIDE {
    deferred_initialize_called_ = true;
    return deferred_initialize_result_;
  }
  virtual void SetNeedsRedrawRect(gfx::Rect damage_rect) OVERRIDE {}
  virtual void OnVSyncParametersChanged(base::TimeTicks timebase,
                                        base::TimeDelta interval) OVERRIDE {}
  virtual void BeginFrame(base::TimeTicks frame_time) OVERRIDE {}
  virtual void OnSwapBuffersComplete(const CompositorFrameAck* ack) OVERRIDE {}
  virtual void DidLoseOutputSurface() OVERRIDE {
    did_lose_output_surface_called_ = true;
  }
  virtual void SetExternalDrawConstraints(const gfx::Transform& transform,
                                          gfx::Rect viewport) OVERRIDE {}

  void set_deferred_initialize_result(bool result) {
    deferred_initialize_result_ = result;
  }

  bool deferred_initialize_called() {
    return deferred_initialize_called_;
  }

  bool did_lose_output_surface_called() {
    return did_lose_output_surface_called_;
  }

 private:
  bool deferred_initialize_result_;
  bool deferred_initialize_called_;
  bool did_lose_output_surface_called_;
};

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientSuccess) {
  scoped_ptr<TestWebGraphicsContext3D> context3d =
      TestWebGraphicsContext3D::Create();

  TestOutputSurface output_surface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>());
  EXPECT_EQ(NULL, output_surface.client());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_EQ(&client, output_surface.client());
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
  EXPECT_EQ(NULL, output_surface.client());

  FakeOutputSurfaceClient client;
  EXPECT_FALSE(output_surface.BindToClient(&client));
  EXPECT_EQ(NULL, output_surface.client());
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
    EXPECT_EQ(&client_, output_surface_.client());
  }

  void InitializeNewContextExpectFail() {
    EXPECT_FALSE(output_surface_.InitializeNewContext3D(
        context3d_.PassAs<WebKit::WebGraphicsContext3D>()));
    EXPECT_EQ(&client_, output_surface_.client());

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

}  // namespace
}  // namespace cc
