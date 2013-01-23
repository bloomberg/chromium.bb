// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SURFACE_H_
#define CC_OUTPUT_SURFACE_H_

#define USE_CC_OUTPUT_SURFACE // TODO(danakj): Remove this.

#include "cc/cc_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorOutputSurface.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class CompositorFrame;
class OutputSurfaceClient;
class SoftwareOutputDevice;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread by the LayerTreeHost through its client.
//   2. Passed to the compositor thread and bound to a client via BindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output
//      surface (on the compositor thread) and go back to step 1.
class CC_EXPORT OutputSurface : public WebKit::WebCompositorOutputSurface {
 public:
  virtual ~OutputSurface() {}

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point on the output surface will only be used on the compositor
  // thread.
  virtual bool BindToClient(OutputSurfaceClient*) = 0;

  struct Capabilities {
    Capabilities()
        : has_parent_compositor(false) {}

    bool has_parent_compositor;
  };

  virtual const Capabilities& Capabilities() const = 0;

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  virtual WebKit::WebGraphicsContext3D* Context3D() const = 0;
  virtual SoftwareOutputDevice* SoftwareDevice() const = 0;

  // Sends frame data to the parent compositor. This should only be called when
  // capabilities().has_parent_compositor. The implementation may destroy or
  // steal the contents of the CompositorFrame passed in.
  virtual void SendFrameToParentCompositor(CompositorFrame*) = 0;

  // Notifies frame-rate smoothness preference. If true, all non-critical
  // processing should be stopped, or lowered in priority.
  virtual void UpdateSmoothnessTakesPriority(bool prefer_smoothness) {}
};

}  // namespace cc

#endif  // CC_OUTPUT_SURFACE_H_
