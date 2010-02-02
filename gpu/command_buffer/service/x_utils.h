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

// This class is a wrapper around an X Window and associated GL context. It is
// useful to isolate intrusive X headers, since it can be forward declared
// (Window and GLXContext can't).
class XWindowWrapper {
 public:
  XWindowWrapper(Display *display, Window window)
      : display_(display),
        window_(window) {
    DCHECK(display_);
    DCHECK(window_);
  }
  // Initializes the GL context.
  bool Initialize();

  // Destroys the GL context.
  void Destroy();

  // Makes the GL context current on the current thread.
  bool MakeCurrent();

  // Swaps front and back buffers.
  void SwapBuffers();

 private:
  Display *display_;
  Window window_;
  GLXContext context_;
  DISALLOW_COPY_AND_ASSIGN(XWindowWrapper);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_X_UTILS_H_
