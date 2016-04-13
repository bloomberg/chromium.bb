// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_BUFFER_QUEUE_H_
#define COMPONENTS_MUS_SURFACES_BUFFER_QUEUE_H_

#include <stddef.h>

#include <deque>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class ContextProvider;
}

namespace gfx {
class GpuMemoryBuffer;
}

class SkRegion;

namespace mus {

class MojoGpuMemoryBufferManager;
class GLHelper;

// Provides a surface that manages its own buffers, backed by GpuMemoryBuffers
// created using CHROMIUM_gpu_memory_buffer_image. Double/triple buffering is
// implemented internally. Doublebuffering occurs if PageFlipComplete is called
// before the next BindFramebuffer call, otherwise it creates extra buffers.
class BufferQueue {
 public:
  BufferQueue(scoped_refptr<cc::ContextProvider> context_provider,
              uint32_t texture_target,
              uint32_t internalformat,
              gfx::AcceleratedWidget widget);
  virtual ~BufferQueue();

  void Initialize();

  void BindFramebuffer();
  void SwapBuffers(const gfx::Rect& damage);
  void PageFlipComplete();
  void Reshape(const gfx::Size& size, float scale_factor);

  void RecreateBuffers();

  unsigned int current_texture_id() const {
    return current_surface_ ? current_surface_->texture : 0;
  }
  unsigned int fbo() const { return fbo_; }

 private:
  friend class AllocatedSurface;

  struct AllocatedSurface {
    AllocatedSurface(BufferQueue* buffer_queue,
                     std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
                     unsigned int texture,
                     unsigned int image,
                     const gfx::Rect& rect);
    ~AllocatedSurface();
    BufferQueue* const buffer_queue;
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer;
    const unsigned int texture;
    const unsigned int image;
    gfx::Rect damage;  // This is the damage for this frame from the previous.
  };

  void FreeAllSurfaces();

  void FreeSurfaceResources(AllocatedSurface* surface);

  // Copy everything that is in |copy_rect|, except for what is in
  // |exclude_rect| from |source_texture| to |texture|.
  virtual void CopyBufferDamage(int texture,
                                int source_texture,
                                const gfx::Rect& new_damage,
                                const gfx::Rect& old_damage);

  void UpdateBufferDamage(const gfx::Rect& damage);

  void CopySubBufferDamage(GLenum target,
                           GLuint texture,
                           GLuint previous_texture,
                           const SkRegion& new_damage,
                           const SkRegion& old_damage);

  // Return a surface, available to be drawn into.
  std::unique_ptr<AllocatedSurface> GetNextSurface();

  std::unique_ptr<AllocatedSurface> RecreateBuffer(
      std::unique_ptr<AllocatedSurface> surface);

  gfx::Size size_;
  scoped_refptr<cc::ContextProvider> context_provider_;

  unsigned int fbo_;
  size_t allocated_count_;
  unsigned int texture_target_;
  unsigned int internal_format_;
  // This surface is currently bound. This may be nullptr if no surface has
  // been bound, or if allocation failed at bind.
  std::unique_ptr<AllocatedSurface> current_surface_;
  // The surface currently on the screen, if any.
  std::unique_ptr<AllocatedSurface> displayed_surface_;
  // These are free for use, and are not nullptr.
  std::vector<std::unique_ptr<AllocatedSurface>> available_surfaces_;
  // These have been swapped but are not displayed yet. Entries of this deque
  // may be nullptr, if they represent frames that have been destroyed.
  std::deque<std::unique_ptr<AllocatedSurface>> in_flight_surfaces_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_BUFFER_QUEUE_H_
