// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeCCGraphicsContext_h
#define FakeCCGraphicsContext_h

#include "CCGraphicsContext.h"
#include "CompositorFakeWebGraphicsContext3D.h"
#include "FakeWebCompositorOutputSurface.h"
#include <public/WebCompositorOutputSurface.h>

namespace WebKit {

static inline PassOwnPtr<cc::CCGraphicsContext> createFakeCCGraphicsContext()
{
    return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes()));
}

} // namespace WebKit

#endif // FakeCCGraphicsContext_h
