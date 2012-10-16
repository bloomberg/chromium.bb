// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeCCGraphicsContext_h
#define FakeCCGraphicsContext_h

#include "CCGraphicsContext.h"
#include "cc/test/compositor_fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include <public/WebCompositorOutputSurface.h>

namespace WebKit {

static inline scoped_ptr<cc::CCGraphicsContext> createFakeCCGraphicsContext()
{
    return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes())).PassAs<cc::CCGraphicsContext>();
}

} // namespace WebKit

#endif // FakeCCGraphicsContext_h
