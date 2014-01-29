// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include "cc/output/output_surface_client.h"
#include "ui/gfx/transform.h"

namespace cc {

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : OutputSurface(context_provider), external_stencil_test_(false) {}

PixelTestOutputSurface::PixelTestOutputSurface(
    scoped_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(software_device.Pass()), external_stencil_test_(false) {}

void PixelTestOutputSurface::Reshape(const gfx::Size& size,
                                     float scale_factor) {
  gfx::Size expanded_size(size.width() + surface_expansion_size_.width(),
                          size.height() + surface_expansion_size_.height());
  OutputSurface::Reshape(expanded_size, scale_factor);
}

bool PixelTestOutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_;
}

}  // namespace cc
