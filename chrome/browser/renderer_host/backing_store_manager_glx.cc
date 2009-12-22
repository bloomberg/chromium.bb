// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store_manager_glx.h"

#include <GL/gl.h>
#include <X11/Xutil.h>

#include "base/scoped_ptr.h"
#include "chrome/common/x11_util.h"

namespace {

// scoped_ptr functor for XFree().
class ScopedPtrXFree {
 public:
  inline void operator()(void* x) const {
    ::XFree(x);
  }
};

}  // namespace

BackingStoreManagerGlx::BackingStoreManagerGlx()
    : display_(x11_util::GetXDisplay()),
      tried_to_init_(false),
      context_(NULL),
      previous_window_id_(0) {
}

BackingStoreManagerGlx::~BackingStoreManagerGlx() {
  if (context_)
    glXDestroyContext(display_, context_);
}

GLXContext BackingStoreManagerGlx::BindContext(XID window_id) {
  if (tried_to_init_) {
    if (!context_)
      return NULL;
    if (!previous_window_id_ || previous_window_id_ != window_id)
      ::glXMakeCurrent(display_, window_id, context_);
    previous_window_id_ = window_id;
    return context_;
  }
  tried_to_init_ = true;

  int attrib_list[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
  scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info(
      ::glXChooseVisual(display_, 0, attrib_list));
  if (!visual_info.get())
    return NULL;

  context_ = ::glXCreateContext(display_, visual_info.get(), NULL, True);
  ::glXMakeCurrent(display_, window_id, context_);
  return context_;
}
