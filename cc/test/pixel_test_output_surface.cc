// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_output_surface.h"

#include "cc/output/output_surface_client.h"
#include "ui/gfx/transform.h"

namespace cc {

void PixelTestOutputSurface::Reshape(gfx::Size size, float scale_factor) {
  gfx::Size expanded_size(size.width() + surface_expansion_size_.width(),
                          size.height() + surface_expansion_size_.height());
  OutputSurface::Reshape(expanded_size, scale_factor);

  gfx::Rect offset_viewport = gfx::Rect(size) + viewport_offset_;
  SetExternalDrawConstraints(gfx::Transform(), offset_viewport);
}

}  // namespace cc
