// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_

#include "cc/output/output_surface.h"

namespace android_webview {

class ParentOutputSurface : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  explicit ParentOutputSurface(
      scoped_refptr<cc::ContextProvider> context_provider);
  ~ParentOutputSurface() override;

  // OutputSurface overrides.
  void Reshape(const gfx::Size& size, float scale_factor) override;
  void SwapBuffers(cc::CompositorFrame* frame) override;
  using cc::OutputSurface::SetExternalStencilTest;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentOutputSurface);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_
