// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_GRAPHICS_CONTEXT_H_
#define CC_TEST_PIXEL_TEST_GRAPHICS_CONTEXT_H_

#include "base/memory/scoped_ptr.h"
#include "cc/output_surface.h"

namespace cc {

class PixelTestOutputSurface : public OutputSurface {
 public:
  static scoped_ptr<PixelTestOutputSurface> create() {
    return make_scoped_ptr(new PixelTestOutputSurface);
  }

  virtual ~PixelTestOutputSurface();

  // OutputSurface overrides.
  virtual bool bindToClient(WebKit::WebCompositorOutputSurfaceClient*) OVERRIDE;
  virtual const WebKit::WebCompositorOutputSurface::Capabilities& capabilities() const OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* context3D() const OVERRIDE;
  virtual void sendFrameToParentCompositor(
      const WebKit::WebCompositorFrame&) OVERRIDE { }

 private:
  PixelTestOutputSurface();

  scoped_ptr<WebKit::WebGraphicsContext3D> context_;
};

}  // namespace cc


#endif  // CC_TEST_PIXEL_TEST_GRAPHICS_CONTEXT_H_
