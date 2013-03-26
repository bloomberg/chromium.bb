// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_H_

#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

class FakeOutputSurface : public OutputSurface {
 public:
  virtual ~FakeOutputSurface();

  static scoped_ptr<FakeOutputSurface> Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), false));
  }

  static scoped_ptr<FakeOutputSurface> Create3d() {
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d =
        TestWebGraphicsContext3D::Create(
            WebKit::WebGraphicsContext3D::Attributes())
        .PassAs<WebKit::WebGraphicsContext3D>();
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), false));
  }

  static scoped_ptr<FakeOutputSurface> CreateSoftware(
      scoped_ptr<SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new FakeOutputSurface(software_device.Pass(), false));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegating3d(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), true));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegating3d() {
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d =
        TestWebGraphicsContext3D::Create(
            WebKit::WebGraphicsContext3D::Attributes())
        .PassAs<WebKit::WebGraphicsContext3D>();
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), true));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegatingSoftware(
      scoped_ptr<SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new FakeOutputSurface(software_device.Pass(), true));
  }

  virtual bool BindToClient(OutputSurfaceClient* client) OVERRIDE;
  virtual void SendFrameToParentCompositor(CompositorFrame* frame) OVERRIDE;

  CompositorFrame& last_sent_frame() { return last_sent_frame_; }
  size_t num_sent_frames() { return num_sent_frames_; }

 private:
  FakeOutputSurface(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
      bool has_parent);

  FakeOutputSurface(
      scoped_ptr<SoftwareOutputDevice> software_device,
      bool has_parent);

  CompositorFrame last_sent_frame_;
  size_t num_sent_frames_;
};

static inline scoped_ptr<cc::OutputSurface> CreateFakeOutputSurface() {
  return FakeOutputSurface::Create3d(
      TestWebGraphicsContext3D::Create(
          WebKit::WebGraphicsContext3D::Attributes())
          .PassAs<WebKit::WebGraphicsContext3D>()).PassAs<cc::OutputSurface>();
}

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_H_
