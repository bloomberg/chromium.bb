// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares the XWindowWrapper class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_X_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_X_UTILS_H_

#include "base/basictypes.h"
#include "gpu/command_buffer/common/logging.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {

// Abstracts between on-screen and off-screen GLX contexts.
class GLXContextWrapper {
 public:
  explicit GLXContextWrapper(Display* display)
      : display_(display),
        context_(NULL) {
    DCHECK(display_);
  }

  virtual ~GLXContextWrapper();

  // Initializes the GL context. Subclasses should perform their
  // initialization work and then call the superclass implementation.
  virtual bool Initialize();

  // Destroys the GL context. Subclasses may call this before or after
  // doing any necessary cleanup work.
  virtual void Destroy();

  // Makes the GL context current on the current thread.
  virtual bool MakeCurrent() = 0;

  // Returns true if this context is offscreen.
  virtual bool IsOffscreen() = 0;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual void SwapBuffers() = 0;

  // Fetches the GLX context for sharing of server-side objects.
  GLXContext GetContext() {
    return context_;
  }

 protected:
  Display* GetDisplay() {
    return display_;
  }

  void SetContext(GLXContext context) {
    context_ = context;
  }

 private:
  Display* display_;
  GLXContext context_;

  DISALLOW_COPY_AND_ASSIGN(GLXContextWrapper);
};

// This class is a wrapper around an X Window and associated GL context. It is
// useful to isolate intrusive X headers, since it can be forward declared
// (Window and GLXContext can't).
class XWindowWrapper : public GLXContextWrapper {
 public:
  XWindowWrapper(Display *display, Window window)
      : GLXContextWrapper(display),
        window_(window) {
    DCHECK(window_);
  }

  virtual bool Initialize();
  virtual bool MakeCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();

 private:
  Window window_;
  DISALLOW_COPY_AND_ASSIGN(XWindowWrapper);
};

// This class is a wrapper around a GLX pbuffer and associated GL context. It
// serves the same purpose as the XWindowWrapper for initializing off-screen
// rendering.
class GLXPbufferWrapper : public GLXContextWrapper {
 public:
  explicit GLXPbufferWrapper(Display* display)
      : GLXContextWrapper(display) {
  }

  virtual bool Initialize();
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();

 private:
  GLXPbuffer pbuffer_;
  DISALLOW_COPY_AND_ASSIGN(GLXPbufferWrapper);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_X_UTILS_H_
