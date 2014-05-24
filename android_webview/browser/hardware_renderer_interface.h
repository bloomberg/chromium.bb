// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_INTERFACE_H_
#define ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_INTERFACE_H_

struct AwDrawGLInfo;

namespace android_webview {

struct DrawGLResult;

class HardwareRendererInterface {
 public:
  virtual ~HardwareRendererInterface() {}

  virtual bool DrawGL(bool stencil_enabled,
                      int framebuffer_binding_ext,
                      AwDrawGLInfo* draw_info,
                      DrawGLResult* result) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HARDWARE_RENDERER_INTERFACE_H_
