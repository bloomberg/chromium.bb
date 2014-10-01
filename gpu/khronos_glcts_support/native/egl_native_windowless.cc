// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Using egl_native from gles2_conform_support
// TODO: We may want to phase out the old gles2_conform support in preference
// of this implementation.  So eventually we'll need to move the egl_native
// stuff here or to a shareable location/path.
#include "gpu/gles2_conform_support/egl/display.h"

#include "third_party/khronos_glcts/framework/egl/tcuEglPlatform.hpp"

namespace egl {
namespace native {
namespace windowless {

class Window : public tcu::NativeWindow {
 public:
  Window(tcu::egl::Display& display,
         EGLConfig config,
         const EGLint* attribList,
         int width,
         int height)
      : tcu::NativeWindow::NativeWindow(),
        eglDisplay_(display),
        eglSurface_(display, config, (EGLNativeWindowType)NULL, attribList) {}

  virtual ~Window() {}

  tcu::egl::Display& getEglDisplay() { return eglDisplay_; }

  tcu::egl::WindowSurface& getEglSurface() { return eglSurface_; }

  void processEvents() { return; }

 private:
  tcu::egl::Display& eglDisplay_;
  tcu::egl::WindowSurface eglSurface_;
};

class Platform : public tcu::EglPlatform {
 public:
  Platform() : tcu::EglPlatform::EglPlatform() {}

  virtual ~Platform() {}

  tcu::NativeWindow* createWindow(tcu::NativeDisplay& dpy,
                                  EGLConfig config,
                                  const EGLint* attribList,
                                  int width,
                                  int height,
                                  qpVisibility visibility) {
    tcu::egl::Display& eglDisplay = dpy.getEglDisplay();
    egl::Display* display =
        static_cast<egl::Display*>(eglDisplay.getEGLDisplay());
    display->SetCreateOffscreen(width, height);
    return new Window(eglDisplay, config, attribList, width, height);
  }
};

}  // namespace windowless
}  // namespace native
}  // namespace egl

tcu::Platform* createPlatform(void) {
  return new egl::native::windowless::Platform();
}
