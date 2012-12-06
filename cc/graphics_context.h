// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_GRAPHICS_CONTEXT_H_
#define CC_GRAPHICS_CONTEXT_H_

#include <public/WebCompositorOutputSurface.h>
#include <public/WebGraphicsContext3D.h>

namespace cc {

// FIXME: rename fully to OutputSurface.
typedef WebKit::WebCompositorOutputSurface GraphicsContext;

}  // namespace cc

#endif  // CC_GRAPHICS_CONTEXT_H_
