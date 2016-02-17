// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_renderer_statics.h"

#include "content/renderer/android/synchronous_compositor_proxy.h"

namespace content {

void SynchronousCompositorSetSkCanvas(SkCanvas* canvas) {
  SynchronousCompositorProxy::SetSkCanvasForDraw(canvas);
}

}  // namespace content
