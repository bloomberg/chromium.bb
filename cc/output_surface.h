// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SURFACE_H_
#define CC_OUTPUT_SURFACE_H_

#define USE_CC_OUTPUT_SURFACE // TODO(danakj): Remove this.

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/software_output_device.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorOutputSurface.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {

class CompositorFrame;
class OutputSurfaceClient;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread by the LayerTreeHost through its client.
//   2. Passed to the compositor thread and bound to a client via BindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output
//      surface (on the compositor thread) and go back to step 1.
class CC_EXPORT OutputSurface : public WebKit::WebCompositorOutputSurface {
 public:
  OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D> context3d);

  OutputSurface(scoped_ptr<cc::SoftwareOutputDevice> software_device);

  OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
                scoped_ptr<cc::SoftwareOutputDevice> software_device);

  virtual ~OutputSurface();

  struct Capabilities {
    Capabilities()
        : has_parent_compositor(false) {}

    bool has_parent_compositor;
  };

  const Capabilities& capabilities() const {
    return capabilities_;
  }

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  WebKit::WebGraphicsContext3D* context3d() const {
    return context3d_.get();
  }

  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point on the output surface will only be used on the compositor
  // thread.
  virtual bool BindToClient(OutputSurfaceClient*);

  // Sends frame data to the parent compositor. This should only be called when
  // capabilities().has_parent_compositor. The implementation may destroy or
  // steal the contents of the CompositorFrame passed in.
  virtual void SendFrameToParentCompositor(CompositorFrame*);

  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  virtual void Reshape(gfx::Size size);

  virtual void BindFramebuffer();

  virtual void PostSubBuffer(gfx::Rect rect);
  virtual void SwapBuffers();

  // Notifies frame-rate smoothness preference. If true, all non-critical
  // processing should be stopped, or lowered in priority.
  virtual void UpdateSmoothnessTakesPriority(bool prefer_smoothness) {}

 protected:
  OutputSurfaceClient* client_;
  struct cc::OutputSurface::Capabilities capabilities_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<cc::SoftwareOutputDevice> software_device_;
  bool has_gl_discard_backbuffer_;
};

}  // namespace cc

#endif  // CC_OUTPUT_SURFACE_H_
