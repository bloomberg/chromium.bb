// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/test/test_web_graphics_context_3d.h"
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
};

class FakeOutputSurfaceClient : public OutputSurfaceClient {
 public:
  virtual void SetNeedsRedrawRect(gfx::Rect damage_rect) OVERRIDE {}
  virtual void OnVSyncParametersChanged(base::TimeTicks timebase,
                                        base::TimeDelta interval) OVERRIDE {}
  virtual void BeginFrame(base::TimeTicks frame_time) OVERRIDE {}
  virtual void OnSendFrameToParentCompositorAck(const CompositorFrameAck& ack)
      OVERRIDE {}
  virtual void OnSwapBuffersComplete() OVERRIDE {}
  virtual void DidLoseOutputSurface() OVERRIDE {}
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

}  // namespace
}  // namespace cc
