// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BUFFERED_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BUFFERED_OUTPUT_SURFACE_H_

#include <queue>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
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
  ~BufferQueue();

  bool Initialize();

  void BindFramebuffer();
  void SwapBuffers();
  void PageFlipComplete();
  void Reshape(const gfx::Size& size, float scale_factor);

  unsigned int current_texture_id() { return current_surface_.texture; }

 private:
  friend class BufferQueueTest;
  struct AllocatedSurface {
    AllocatedSurface() : texture(0), image(0) {}
    AllocatedSurface(unsigned int texture, unsigned int image)
        : texture(texture), image(image) {}
    unsigned int texture;
    unsigned int image;
  };

  void FreeAllSurfaces();

  void FreeSurface(AllocatedSurface* surface);

  // Return a surface, available to be drawn into.
  AllocatedSurface GetNextSurface();

  gfx::Size size_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  unsigned int fbo_;
  size_t allocated_count_;
  unsigned int internalformat_;
  AllocatedSurface current_surface_;  // This surface is currently bound.
  std::vector<AllocatedSurface> available_surfaces_;  // These are free for use.
  std::queue<AllocatedSurface> in_flight_surfaces_;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BUFFERED_OUTPUT_SURFACE_H_
