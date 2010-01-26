// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_backing_store_glx_context.h"

#include "base/scoped_ptr.h"
#include "chrome/common/x11_util.h"
#include "chrome/gpu/gpu_thread.h"

// Must be last.
#include <GL/glew.h>
#include <GL/glxew.h>
#include <X11/Xutil.h>

GpuBackingStoreGLXContext::GpuBackingStoreGLXContext(GpuThread* gpu_thread)
    : gpu_thread_(gpu_thread),
      tried_to_init_(false),
      context_(NULL),
      previous_window_id_(0) {
}

GpuBackingStoreGLXContext::~GpuBackingStoreGLXContext() {
  if (context_)
    glXDestroyContext(gpu_thread_->display(), context_);
}

GLXContext GpuBackingStoreGLXContext::BindContext(XID window_id) {
  if (tried_to_init_) {
    if (!context_)
      return NULL;
    if (!previous_window_id_ || previous_window_id_ != window_id) {
      bool success = ::glXMakeCurrent(gpu_thread_->display(), window_id,
                                      context_);
      DCHECK(success);
    }
    previous_window_id_ = window_id;
    return context_;
  }
  tried_to_init_ = true;

  int attrib_list[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
  scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info(
      ::glXChooseVisual(gpu_thread_->display(), 0, attrib_list));
  if (!visual_info.get())
    return NULL;

  context_ = ::glXCreateContext(gpu_thread_->display(), visual_info.get(),
                                NULL, True);
  bool success = ::glXMakeCurrent(gpu_thread_->display(), window_id, context_);
  DCHECK(success);
  glewInit();
  return context_;
}
