// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCGraphicsContext_h
#define CCGraphicsContext_h

#include <public/WebCompositorOutputSurface.h>
#include <public/WebGraphicsContext3D.h>

namespace cc {

// FIXME: rename fully to CCOutputSurface.
typedef WebKit::WebCompositorOutputSurface CCGraphicsContext;

}  // namespace cc

#endif  // CCGraphicsContext_h
