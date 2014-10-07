// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_GL_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_GL_SURFACE_H_

#include "ui/gl/gl_surface.h"

namespace android_webview {

// This surface is used to represent the underlying surface provided by the App
// inside a hardware draw. Note that offscreen contexts will not be using this
// GLSurface.
class GL_EXPORT AwGLSurface : public gfx::GLSurface {
 public:
  AwGLSurface();

  // Implement GLSurface.
  virtual void Destroy() override;
  virtual bool IsOffscreen() override;
  virtual unsigned int GetBackingFrameBufferObject() override;
  virtual bool SwapBuffers() override;
  virtual gfx::Size GetSize() override;
  virtual void* GetHandle() override;
  virtual void* GetDisplay() override;

  void SetBackingFrameBufferObject(unsigned int fbo);
  void ResetBackingFrameBufferObject();

 protected:
  virtual ~AwGLSurface();

 private:
  unsigned int fbo_;

  DISALLOW_COPY_AND_ASSIGN(AwGLSurface);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_GL_SURFACE_H_
