// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_

#include <queue>

#include "android_webview/browser/shared_renderer_state.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_local.h"

struct AwDrawGLInfo;

namespace android_webview {

class AwGLSurface;
class BrowserViewRendererClient;

namespace internal {
class DeferredGpuCommandService;
}  // namespace internal

class HardwareRenderer {
 public:
  explicit HardwareRenderer(SharedRendererState* state);
  ~HardwareRenderer();

  bool DrawGL(bool stencil_enabled,
              int framebuffer_binding_ext,
              AwDrawGLInfo* draw_info,
              DrawGLResult* result);

 private:
  friend class internal::DeferredGpuCommandService;

  void SetCompositorMemoryPolicy();

  SharedRendererState* shared_renderer_state_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  scoped_refptr<AwGLSurface> gl_surface_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_H_
