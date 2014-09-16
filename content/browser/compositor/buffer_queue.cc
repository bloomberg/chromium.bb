// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/buffer_queue.h"

#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace content {

BufferQueue::BufferQueue(scoped_refptr<cc::ContextProvider> context_provider,
                         unsigned int internalformat)
    : context_provider_(context_provider),
      fbo_(0),
      allocated_count_(0),
      internalformat_(internalformat) {
}

BufferQueue::~BufferQueue() {
  FreeAllSurfaces();

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (fbo_)
    gl->DeleteFramebuffers(1, &fbo_);
}

bool BufferQueue::Initialize() {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenFramebuffers(1, &fbo_);
  return fbo_ != 0;
}

void BufferQueue::BindFramebuffer() {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  if (!current_surface_.texture) {
    current_surface_ = GetNextSurface();
    gl->FramebufferTexture2D(GL_FRAMEBUFFER,
                             GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D,
                             current_surface_.texture,
                             0);
  }
}

void BufferQueue::SwapBuffers() {
  in_flight_surfaces_.push(current_surface_);
  current_surface_.texture = 0;
  current_surface_.image = 0;
}

void BufferQueue::Reshape(const gfx::Size& size, float scale_factor) {
  DCHECK(!current_surface_.texture);
  if (size == size_)
    return;
  size_ = size;

  // TODO: add stencil buffer when needed.
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  gl->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

  FreeAllSurfaces();
}

void BufferQueue::PageFlipComplete() {
  if (in_flight_surfaces_.size() > 1) {
    available_surfaces_.push_back(in_flight_surfaces_.front());
    in_flight_surfaces_.pop();
  }
}

void BufferQueue::FreeAllSurfaces() {
  FreeSurface(&current_surface_);
  while (!in_flight_surfaces_.empty()) {
    FreeSurface(&in_flight_surfaces_.front());
    in_flight_surfaces_.pop();
  }
  for (size_t i = 0; i < available_surfaces_.size(); i++)
    FreeSurface(&available_surfaces_[i]);
  available_surfaces_.clear();
}

void BufferQueue::FreeSurface(AllocatedSurface* surface) {
  if (surface->texture) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindTexture(GL_TEXTURE_2D, surface->texture);
    gl->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, surface->image);
    gl->DeleteTextures(1, &surface->texture);
    gl->DestroyImageCHROMIUM(surface->image);
    surface->image = 0;
    surface->texture = 0;
    allocated_count_--;
  }
}

BufferQueue::AllocatedSurface BufferQueue::GetNextSurface() {
  if (!available_surfaces_.empty()) {
    AllocatedSurface id = available_surfaces_.back();
    available_surfaces_.pop_back();
    return id;
  }

  unsigned int texture = 0;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, &texture);
  if (!texture)
    return AllocatedSurface();

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, 4U);

  unsigned int id = context_provider_->ContextGL()->CreateImageCHROMIUM(
      size_.width(),
      size_.height(),
      internalformat_,
      GL_IMAGE_SCANOUT_CHROMIUM);
  allocated_count_++;
  gl->BindTexture(GL_TEXTURE_2D, texture);
  gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, id);
  return AllocatedSurface(texture, id);
}

}  // namespace content
