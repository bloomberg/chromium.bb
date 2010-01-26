// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_BACKING_STORE_GLX_CONTEXT_H_
#define CHROME_GPU_GPU_BACKING_STORE_GLX_CONTEXT_H_

#include "base/basictypes.h"
#include "chrome/gpu/x_util.h"

class GpuThread;

class GpuBackingStoreGLXContext {
 public:
  GpuBackingStoreGLXContext(GpuThread* gpu_thread);
  ~GpuBackingStoreGLXContext();

  // Returns the context, creating it if necessary, and binding it to the given
  // display and window identified by the XID. This will avoid duplicate calls
  // to MakeCurrent if the display/XID hasn't changed from the last call.
  // Returns NULL on failure.
  GLXContext BindContext(XID window_id);

 private:
  GpuThread* gpu_thread_;

  // Set to true when we've tried to create the context. This prevents us from
  // trying to initialize the OpenGL context over and over in the failure case.
  bool tried_to_init_;

  // The OpenGL context. Non-NULL when initialized.
  GLXContext context_;

  // The last window we've bound our context to. This allows us to avoid
  // duplicate "MakeCurrent" calls which are expensive.
  XID previous_window_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuBackingStoreGLXContext);
};

#endif  // CHROME_GPU_GPU_BACKING_STORE_GLX_CONTEXT_H_
