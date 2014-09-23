// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BUFFERED_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BUFFERED_OUTPUT_SURFACE_H_

#include <queue>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {
class ContextProvider;
}

namespace content {

// Provides a surface that manages its own uffers, backed by
// CreateImageCHROMIUM. Double/triple buffering is implemented
// internally. Doublebuffering occurs if PageFlipComplete is called before
// the next BindFramebuffer call, otherwise it creates extra buffers.
class CONTENT_EXPORT BufferQueue {
 public:
  BufferQueue(scoped_refptr<cc::ContextProvider> context_provider,
              unsigned int internalformat);
  virtual ~BufferQueue();

  bool Initialize();

  void BindFramebuffer();
  void SwapBuffers(const gfx::Rect& damage);
  void PageFlipComplete();
  void Reshape(const gfx::Size& size, float scale_factor);

  unsigned int current_texture_id() { return current_surface_.texture; }

 private:
  friend class BufferQueueTest;

  struct AllocatedSurface {
    AllocatedSurface() : texture(0), image(0) {}
    AllocatedSurface(unsigned int texture,
                     unsigned int image,
                     const gfx::Rect& rect)
        : texture(texture), image(image), damage(rect) {}
    unsigned int texture;
    unsigned int image;
    gfx::Rect damage;  // This is the damage for this frame from the previous.
  };

  void FreeAllSurfaces();

  void FreeSurface(AllocatedSurface* surface);

  // Copy everything that is in |copy_rect|, except for what is in
  // |exclude_rect| from |source_texture| to |texture|.
  virtual void CopyBufferDamage(int texture,
                                int source_texture,
                                const gfx::Rect& new_damage,
                                const gfx::Rect& old_damage);

  void UpdateBufferDamage(const gfx::Rect& damage);

  // Return a surface, available to be drawn into.
  AllocatedSurface GetNextSurface();

  gfx::Size size_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  unsigned int fbo_;
  size_t allocated_count_;
  unsigned int internalformat_;
  AllocatedSurface current_surface_;  // This surface is currently bound.
  std::vector<AllocatedSurface> available_surfaces_;  // These are free for use.
  std::deque<AllocatedSurface> in_flight_surfaces_;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BUFFERED_OUTPUT_SURFACE_H_
