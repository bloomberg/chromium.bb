// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_H_

#include "cc/output_surface.h"
#include "cc/test/compositor_fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include <public/WebCompositorOutputSurface.h>

namespace cc {

static inline scoped_ptr<cc::OutputSurface> createFakeOutputSurface()
{
    return WebKit::FakeWebCompositorOutputSurface::create(WebKit::CompositorFakeWebGraphicsContext3D::create(WebKit::WebGraphicsContext3D::Attributes()).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<cc::OutputSurface>();
}

} // namespace WebKit

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_H_
