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
      previous_window_id_(0),
      frame_buffer_for_scrolling_(0),
      is_frame_buffer_bound_(false),
      temp_scroll_texture_id_(0) {
}

GpuBackingStoreGLXContext::~GpuBackingStoreGLXContext() {
  if (temp_scroll_texture_id_) {
    glDeleteTextures(1, &temp_scroll_texture_id_);
    temp_scroll_texture_id_ = 0;
  }

  if (frame_buffer_for_scrolling_)
    glDeleteFramebuffers(1, &frame_buffer_for_scrolling_);

  if (context_)
    glXDestroyContext(gpu_thread_->display(), context_);
}

GLXContext GpuBackingStoreGLXContext::BindContext(XID window_id) {
  DCHECK(!is_frame_buffer_bound_);

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

bool GpuBackingStoreGLXContext::BindTextureForScrolling(
    XID window_id,
    const gfx::Size& size) {
  DCHECK(!is_frame_buffer_bound_);
  BindContext(window_id);

  // Create a new destination texture if the old one isn't properly sized.
  // This means we try to re-use old ones without re-creating all the texture
  // to save work in the common case of scrolling a window repeatedly that
  // doesn't change size.
  if (temp_scroll_texture_id_ == 0 ||
      size != temp_scroll_texture_size_) {
    if (!temp_scroll_texture_id_) {
      // There may be no temporary one created yet.
      glGenTextures(1, &temp_scroll_texture_id_);
    }

    // Create a new texture in the context with random garbage in it large
    // enough to fit the required size.
    glBindTexture(GL_TEXTURE_2D, temp_scroll_texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(),
                 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  if (!frame_buffer_for_scrolling_)
    glGenFramebuffers(1, &frame_buffer_for_scrolling_);
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_for_scrolling_);
  is_frame_buffer_bound_ = true;

  // Release our color attachment.
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         temp_scroll_texture_id_, 0);

  DCHECK(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) ==
         GL_FRAMEBUFFER_COMPLETE_EXT);
  return true;
}

unsigned int GpuBackingStoreGLXContext::SwapTextureForScrolling(
    unsigned int old_texture,
    const gfx::Size& old_size) {
  // Unbind the framebuffer, which we expect to be bound.
  DCHECK(is_frame_buffer_bound_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  is_frame_buffer_bound_ = false;

  DCHECK(temp_scroll_texture_id_);
  unsigned int new_texture = temp_scroll_texture_id_;

  temp_scroll_texture_id_ = old_texture;
  temp_scroll_texture_size_ = old_size;

  return new_texture;
}
