// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/buffer_queue.h"

#include "content/browser/compositor/image_transport_factory.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"

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

void BufferQueue::CopyBufferDamage(int texture,
                                   int source_texture,
                                   const gfx::Rect& new_damage,
                                   const gfx::Rect& old_damage) {
  ImageTransportFactory::GetInstance()->GetGLHelper()->CopySubBufferDamage(
      texture,
      source_texture,
      SkRegion(SkIRect::MakeXYWH(new_damage.x(),
                                 new_damage.y(),
                                 new_damage.width(),
                                 new_damage.height())),
      SkRegion(SkIRect::MakeXYWH(old_damage.x(),
                                 old_damage.y(),
                                 old_damage.width(),
                                 old_damage.height())));
}

void BufferQueue::UpdateBufferDamage(const gfx::Rect& damage) {
  for (size_t i = 0; i < available_surfaces_.size(); i++)
    available_surfaces_[i].damage.Union(damage);
  for (std::deque<AllocatedSurface>::iterator it =
           in_flight_surfaces_.begin();
       it != in_flight_surfaces_.end();
       ++it)
    it->damage.Union(damage);
}

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  if (damage != gfx::Rect(size_)) {
    // We must have a frame available to copy from.
    DCHECK(!in_flight_surfaces_.empty());
    CopyBufferDamage(current_surface_.texture,
                     in_flight_surfaces_.back().texture,
                     damage,
                     current_surface_.damage);
  }
  UpdateBufferDamage(damage);
  current_surface_.damage = gfx::Rect();
  in_flight_surfaces_.push_back(current_surface_);
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
    in_flight_surfaces_.pop_front();
  }
}

void BufferQueue::FreeAllSurfaces() {
  FreeSurface(&current_surface_);
  while (!in_flight_surfaces_.empty()) {
    FreeSurface(&in_flight_surfaces_.front());
    in_flight_surfaces_.pop_front();
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
    AllocatedSurface surface = available_surfaces_.back();
    available_surfaces_.pop_back();
    return surface;
  }

  unsigned int texture = 0;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, &texture);
  if (!texture)
    return AllocatedSurface();

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, 4U);

  unsigned int id = gl->CreateImageCHROMIUM(
      size_.width(),
      size_.height(),
      internalformat_,
      GL_IMAGE_SCANOUT_CHROMIUM);
  if (!id) {
    LOG(ERROR) << "Failed to allocate backing CreateImageCHROMIUM surface";
    gl->DeleteTextures(1, &texture);
    return AllocatedSurface();
  }
  allocated_count_++;
  gl->BindTexture(GL_TEXTURE_2D, texture);
  gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, id);
  return AllocatedSurface(texture, id, gfx::Rect(size_));
}

}  // namespace content
