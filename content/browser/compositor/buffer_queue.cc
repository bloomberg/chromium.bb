// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/buffer_queue.h"

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "cc/output/context_provider.h"
#include "content/browser/compositor/gl_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/skia_util.h"

namespace content {

BufferQueue::BufferQueue(scoped_refptr<cc::ContextProvider> context_provider,
                         unsigned int texture_target,
                         unsigned int internalformat,
                         GLHelper* gl_helper,
                         gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
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

  if (!current_surface_)
    current_surface_ = GetNextSurface();

  if (current_surface_) {
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             texture_target_, current_surface_->texture, 0);
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
  if (displayed_surface_)
    displayed_surface_->damage.Union(damage);
  for (auto& surface : available_surfaces_)
    surface->damage.Union(damage);
  for (auto& surface : in_flight_surfaces_) {
    if (surface)
      surface->damage.Union(damage);
  }
}

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  if (current_surface_) {
    if (damage != gfx::Rect(size_)) {
      // Copy damage from the most recently swapped buffer. In the event that
      // the buffer was destroyed and failed to recreate, pick from the most
      // recently available buffer.
      unsigned int texture_id = 0;
      for (auto& surface : base::Reversed(in_flight_surfaces_)) {
        if (surface) {
          texture_id = surface->texture;
          break;
        }
      }
      if (!texture_id && displayed_surface_)
        texture_id = displayed_surface_->texture;

      if (texture_id) {
        CopyBufferDamage(current_surface_->texture, texture_id, damage,
                         current_surface_->damage);
      }
    }
    current_surface_->damage = gfx::Rect();
  }
  UpdateBufferDamage(damage);
  in_flight_surfaces_.push_back(std::move(current_surface_));
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
  DCHECK(!current_surface_);
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
  available_surfaces_.clear();

  // Recreate all in-flight surfaces and put the recreated copies in the queue.
  for (auto& surface : in_flight_surfaces_)
    surface = RecreateBuffer(std::move(surface));

  current_surface_ = RecreateBuffer(std::move(current_surface_));
  displayed_surface_ = RecreateBuffer(std::move(displayed_surface_));

  if (current_surface_) {
    // If we have a texture bound, we will need to re-bind it.
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             texture_target_, current_surface_->texture, 0);
  }
}

std::unique_ptr<BufferQueue::AllocatedSurface> BufferQueue::RecreateBuffer(
    std::unique_ptr<AllocatedSurface> surface) {
  if (!surface)
    return nullptr;

  std::unique_ptr<AllocatedSurface> new_surface(GetNextSurface());
  if (!new_surface)
    return nullptr;

  new_surface->damage = surface->damage;

  // Copy the entire texture.
  CopyBufferDamage(new_surface->texture, surface->texture, gfx::Rect(),
                   gfx::Rect(size_));
  return new_surface;
}

void BufferQueue::PageFlipComplete() {
  DCHECK(!in_flight_surfaces_.empty());
  if (displayed_surface_)
    available_surfaces_.push_back(std::move(displayed_surface_));
  displayed_surface_ = std::move(in_flight_surfaces_.front());
  in_flight_surfaces_.pop_front();
}

void BufferQueue::FreeAllSurfaces() {
  displayed_surface_.reset();
  current_surface_.reset();
  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& surface : in_flight_surfaces_)
    surface = nullptr;
  available_surfaces_.clear();
}

void BufferQueue::FreeSurfaceResources(AllocatedSurface* surface) {
  if (!surface->texture)
    return;

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->BindTexture(texture_target_, surface->texture);
  gl->ReleaseTexImage2DCHROMIUM(texture_target_, surface->image);
  gl->DeleteTextures(1, &surface->texture);
  gl->DestroyImageCHROMIUM(surface->image);
  surface->buffer.reset();
  allocated_count_--;
}

std::unique_ptr<BufferQueue::AllocatedSurface> BufferQueue::GetNextSurface() {
  if (!available_surfaces_.empty()) {
    std::unique_ptr<AllocatedSurface> surface =
        std::move(available_surfaces_.back());
    available_surfaces_.pop_back();
    return surface;
  }

  unsigned int texture = 0;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, &texture);
  if (!texture)
    return nullptr;

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, 4U);

  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      gpu_memory_buffer_manager_->AllocateGpuMemoryBuffer(
          size_, gpu::DefaultBufferFormatForImageFormat(internal_format_),
          gfx::BufferUsage::SCANOUT, surface_id_));
  if (!buffer.get()) {
    gl->DeleteTextures(1, &texture);
    DLOG(ERROR) << "Failed to allocate GPU memory buffer";
    return nullptr;
  }

  unsigned int id = gl->CreateImageCHROMIUM(
      buffer->AsClientBuffer(), size_.width(), size_.height(),
      internal_format_);
  if (!id) {
    LOG(ERROR) << "Failed to allocate backing image surface";
    gl->DeleteTextures(1, &texture);
    return nullptr;
  }

  allocated_count_++;
  gl->BindTexture(texture_target_, texture);
  gl->BindTexImage2DCHROMIUM(texture_target_, id);
  return base::WrapUnique(new AllocatedSurface(this, std::move(buffer), texture,
                                               id, gfx::Rect(size_)));
}

BufferQueue::AllocatedSurface::AllocatedSurface(
    BufferQueue* buffer_queue,
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
    unsigned int texture,
    unsigned int image,
    const gfx::Rect& rect)
    : buffer_queue(buffer_queue),
      buffer(buffer.release()),
      texture(texture),
      image(image),
      damage(rect) {}

BufferQueue::AllocatedSurface::~AllocatedSurface() {
  buffer_queue->FreeSurfaceResources(this);
}

}  // namespace content
