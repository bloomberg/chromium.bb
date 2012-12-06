// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SURFACE_H_
#define CC_OUTPUT_SURFACE_H_

#include <public/WebCompositorOutputSurface.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

// TODO(danakj): Move WebCompositorOutputSurface implementation to here.
typedef WebKit::WebCompositorOutputSurface OutputSurface;

}  // namespace cc

#endif  // CC_OUTPUT_SURFACE_H_
