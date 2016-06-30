// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_

#include "android_webview/browser/scoped_app_gl_state_restore.h"
#include "base/macros.h"
#include "cc/output/output_surface.h"

namespace android_webview {
class AwRenderThreadContextProvider;

class ParentOutputSurface : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  explicit ParentOutputSurface(
      scoped_refptr<AwRenderThreadContextProvider> context_provider);
  ~ParentOutputSurface() override;

  // OutputSurface overrides.
  void DidLoseOutputSurface() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  void SwapBuffers(cc::CompositorFrame frame) override;
  void ApplyExternalStencil() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  void SetGLState(const ScopedAppGLStateRestore& gl_state);

 private:
  StencilState stencil_state_;

  DISALLOW_COPY_AND_ASSIGN(ParentOutputSurface);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_
