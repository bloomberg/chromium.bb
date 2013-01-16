// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/compositor_frame.h"
#include "cc/output_surface.h"
#include "cc/test/fake_software_output_device.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

class FakeOutputSurface : public OutputSurface {
 public:
  virtual ~FakeOutputSurface();

  static inline scoped_ptr<FakeOutputSurface> Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), false));
  }

  static inline scoped_ptr<FakeOutputSurface> Create3d() {
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d =
        FakeWebGraphicsContext3D::Create(
            WebKit::WebGraphicsContext3D::Attributes())
        .PassAs<WebKit::WebGraphicsContext3D>();
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), false));
  }

  static inline scoped_ptr<FakeOutputSurface> CreateSoftware(
      scoped_ptr<SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new FakeOutputSurface(software_device.Pass(), false));
  }

  static inline scoped_ptr<FakeOutputSurface> CreateDelegating3d(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), true));
  }

  static inline scoped_ptr<FakeOutputSurface> CreateDelegating3d() {
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d =
        FakeWebGraphicsContext3D::Create(
            WebKit::WebGraphicsContext3D::Attributes())
        .PassAs<WebKit::WebGraphicsContext3D>();
    return make_scoped_ptr(new FakeOutputSurface(context3d.Pass(), true));
  }

  static inline scoped_ptr<FakeOutputSurface> CreateDelegatingSoftware(
      scoped_ptr<SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new FakeOutputSurface(software_device.Pass(), true));
  }

  virtual bool BindToClient(OutputSurfaceClient* client) OVERRIDE;

  virtual const struct Capabilities& Capabilities() const OVERRIDE;

  virtual WebKit::WebGraphicsContext3D* Context3D() const OVERRIDE;
  virtual SoftwareOutputDevice* SoftwareDevice() const OVERRIDE;

  virtual void SendFrameToParentCompositor(CompositorFrame*) OVERRIDE;

  CompositorFrame& last_sent_frame() { return last_sent_frame_; }
  size_t num_sent_frames() { return num_sent_frames_; }

private:
  explicit FakeOutputSurface(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d, bool has_parent);
  explicit FakeOutputSurface(
      scoped_ptr<SoftwareOutputDevice> software_device, bool has_parent);

  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<SoftwareOutputDevice> software_device_;
  struct Capabilities capabilities_;
  OutputSurfaceClient* client_;
  CompositorFrame last_sent_frame_;
  size_t num_sent_frames_;
};

static inline scoped_ptr<cc::OutputSurface> createFakeOutputSurface()
{
    return FakeOutputSurface::Create3d(
        FakeWebGraphicsContext3D::Create(
            WebKit::WebGraphicsContext3D::Attributes())
        .PassAs<WebKit::WebGraphicsContext3D>())
        .PassAs<cc::OutputSurface>();
}

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_H_
