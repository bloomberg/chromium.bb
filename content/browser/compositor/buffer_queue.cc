// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/buffer_queue.h"

#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/skia_util.h"

namespace content {

BufferQueue::BufferQueue(
    scoped_refptr<cc::ContextProvider> context_provider,
    unsigned int texture_target,
    unsigned int internalformat,
    GLHelper* gl_helper,
    BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager,
    int surface_id)
    : context_provider_(context_provider),
      fbo_(0),
      allocated_count_(0),
      texture_target_(texture_target),
      internal_format_(internalformat),
      gl_helper_(gl_helper),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      surface_id_(surface_id) {}

BufferQueue::~BufferQueue() {
  FreeAllSurfaces();

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (fbo_)
    gl->DeleteFramebuffers(1, &fbo_);
}

void BufferQueue::Initialize() {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenFramebuffers(1, &fbo_);
}

void BufferQueue::BindFramebuffer() {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  if (!current_surface_.texture) {
    current_surface_ = GetNextSurface();
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             texture_target_, current_surface_.texture, 0);
  }
}

void BufferQueue::CopyBufferDamage(int texture,
                                   int source_texture,
                                   const gfx::Rect& new_damage,
                                   const gfx::Rect& old_damage) {
  gl_helper_->CopySubBufferDamage(
      texture_target_, texture, source_texture,
      SkRegion(gfx::RectToSkIRect(new_damage)),
      SkRegion(gfx::RectToSkIRect(old_damage)));
}

void BufferQueue::UpdateBufferDamage(const gfx::Rect& damage) {
  displayed_surface_.damage.Union(damage);
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
    DCHECK(!in_flight_surfaces_.empty() || displayed_surface_.texture);
    unsigned int texture_id = !in_flight_surfaces_.empty()
                                  ? in_flight_surfaces_.back().texture
                                  : displayed_surface_.texture;

    CopyBufferDamage(current_surface_.texture, texture_id, damage,
                     current_surface_.damage);
  }
  UpdateBufferDamage(damage);
  current_surface_.damage = gfx::Rect();
  in_flight_surfaces_.push_back(current_surface_);
  current_surface_.texture = 0;
  current_surface_.image = 0;
  // Some things reset the framebuffer (CopySubBufferDamage, some GLRenderer
  // paths), so ensure we restore it here.
  context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void BufferQueue::Reshape(const gfx::Size& size, float scale_factor) {
  if (size == size_)
    return;
  // TODO(ccameron): This assert is being hit on Mac try jobs. Determine if that
  // is cause for concern or if it is benign.
  // http://crbug.com/524624
#if !defined(OS_MACOSX)
  DCHECK(!current_surface_.texture);
#endif
  size_ = size;

  // TODO: add stencil buffer when needed.
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           texture_target_, 0, 0);

  FreeAllSurfaces();
}

void BufferQueue::RecreateBuffers() {
  // We need to recreate the buffers, for whatever reason the old ones are not
  // presentable on the device anymore.
  // Unused buffers can be freed directly, they will be re-allocated as needed.
  // Any in flight, current or displayed surface must be replaced.
  for (size_t i = 0; i < available_surfaces_.size(); i++)
    FreeSurface(&available_surfaces_[i]);
  available_surfaces_.clear();

  for (auto& surface : in_flight_surfaces_)
    surface = RecreateBuffer(&surface);

  current_surface_ = RecreateBuffer(&current_surface_);
  displayed_surface_ = RecreateBuffer(&displayed_surface_);

  if (current_surface_.texture) {
    // If we have a texture bound, we will need to re-bind it.
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             texture_target_, current_surface_.texture, 0);
  }
}

BufferQueue::AllocatedSurface BufferQueue::RecreateBuffer(
    AllocatedSurface* surface) {
  if (!surface->texture)
    return *surface;
  AllocatedSurface new_surface = GetNextSurface();
  new_surface.damage = surface->damage;

  // Copy the entire texture.
  CopyBufferDamage(new_surface.texture, surface->texture, gfx::Rect(),
                   gfx::Rect(size_));

  FreeSurface(surface);
  return new_surface;
}

void BufferQueue::PageFlipComplete() {
  DCHECK(!in_flight_surfaces_.empty());

  if (displayed_surface_.texture)
    available_surfaces_.push_back(displayed_surface_);

  displayed_surface_ = in_flight_surfaces_.front();
  in_flight_surfaces_.pop_front();
}

void BufferQueue::FreeAllSurfaces() {
  FreeSurface(&displayed_surface_);
  FreeSurface(&current_surface_);
  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& surface : in_flight_surfaces_)
    FreeSurface(&surface);

  for (size_t i = 0; i < available_surfaces_.size(); i++)
    FreeSurface(&available_surfaces_[i]);

  available_surfaces_.clear();
}

void BufferQueue::FreeSurface(AllocatedSurface* surface) {
  if (surface->texture) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindTexture(texture_target_, surface->texture);
    gl->ReleaseTexImage2DCHROMIUM(texture_target_, surface->image);
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

  scoped_ptr<gfx::GpuMemoryBuffer> buffer(
      gpu_memory_buffer_manager_->AllocateGpuMemoryBufferForScanout(
          size_, gpu::ImageFactory::DefaultBufferFormatForImageFormat(
                     internal_format_),
          surface_id_));
  if (!buffer.get()) {
    gl->DeleteTextures(1, &texture);
    DLOG(ERROR) << "Failed to allocate GPU memory buffer";
    return AllocatedSurface();
  }

  unsigned int id = gl->CreateImageCHROMIUM(
      buffer->AsClientBuffer(), size_.width(), size_.height(),
      internal_format_);

  if (!id) {
    LOG(ERROR) << "Failed to allocate backing image surface";
    gl->DeleteTextures(1, &texture);
    return AllocatedSurface();
  }
  allocated_count_++;
  gl->BindTexture(texture_target_, texture);
  gl->BindTexImage2DCHROMIUM(texture_target_, id);
  return AllocatedSurface(buffer.Pass(), texture, id, gfx::Rect(size_));
}

BufferQueue::AllocatedSurface::AllocatedSurface() : texture(0), image(0) {}

BufferQueue::AllocatedSurface::AllocatedSurface(
    scoped_ptr<gfx::GpuMemoryBuffer> buffer,
    unsigned int texture,
    unsigned int image,
    const gfx::Rect& rect)
    : buffer(buffer.release()), texture(texture), image(image), damage(rect) {}

BufferQueue::AllocatedSurface::~AllocatedSurface() {}

}  // namespace content
