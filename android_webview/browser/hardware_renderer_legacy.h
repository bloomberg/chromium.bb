// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_LEGACY_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_LEGACY_H_

#include "android_webview/browser/hardware_renderer_interface.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

struct AwDrawGLInfo;

namespace android_webview {

class AwGLSurface;
class SharedRendererState;
struct DrawGLInput;

class HardwareRendererLegacy : public HardwareRendererInterface {
 public:
  explicit HardwareRendererLegacy(SharedRendererState* state);
  virtual ~HardwareRendererLegacy();

  virtual bool DrawGL(bool stencil_enabled,
                      int framebuffer_binding_ext,
                      AwDrawGLInfo* draw_info,
                      DrawGLResult* result) OVERRIDE;

 private:
  void SetCompositorMemoryPolicy();

  SharedRendererState* shared_renderer_state_;
  scoped_ptr<DrawGLInput> draw_gl_input_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  scoped_refptr<AwGLSurface> gl_surface_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRendererLegacy);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_LEGACY_H_
