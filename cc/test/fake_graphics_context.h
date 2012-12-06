// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_GRAPHICS_CONTEXT_H_
#define CC_TEST_FAKE_GRAPHICS_CONTEXT_H_

#include "cc/graphics_context.h"
#include "cc/test/compositor_fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include <public/WebCompositorOutputSurface.h>

namespace WebKit {

static inline scoped_ptr<cc::GraphicsContext> createFakeGraphicsContext()
{
    return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes()).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<cc::GraphicsContext>();
}

} // namespace WebKit

#endif  // CC_TEST_FAKE_GRAPHICS_CONTEXT_H_
