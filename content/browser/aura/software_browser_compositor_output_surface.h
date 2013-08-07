// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_SOFTWARE_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_AURA_SOFTWARE_OUTPUT_SURFACE_H_

#include "cc/output/output_surface.h"

namespace cc { class SoftwareOutputDevice; }

namespace content {

// TODO(danakj): Inherit from BrowserCompositorOutputSurface to share stuff like
// reflectors, when we split the GL-specific stuff out of the class.
class SoftwareBrowserCompositorOutputSurface : public cc::OutputSurface {
 public:
  static scoped_ptr<SoftwareBrowserCompositorOutputSurface> Create(
      scoped_ptr<cc::SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new SoftwareBrowserCompositorOutputSurface(software_device.Pass()));
  }

 private:
  explicit SoftwareBrowserCompositorOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device);

  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_SOFTWARE_OUTPUT_SURFACE_H_
