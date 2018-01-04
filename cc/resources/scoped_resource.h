// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SCOPED_RESOURCE_H_
#define CC_RESOURCES_SCOPED_RESOURCE_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/resources/resource.h"
#include "components/viz/common/resources/resource_texture_hint.h"
#include "ui/gfx/buffer_types.h"

#if DCHECK_IS_ON()
#include "base/threading/platform_thread.h"
#endif

namespace cc {
class LayerTreeResourceProvider;

class CC_EXPORT ScopedResource : public Resource {
 public:
  explicit ScopedResource(LayerTreeResourceProvider* provider);
  virtual ~ScopedResource();

  void AllocateSoftware(const gfx::Size& size,
                        const gfx::ColorSpace& color_space);
  void AllocateGpuTexture(const gfx::Size& size,
                          viz::ResourceTextureHint hint,
                          viz::ResourceFormat format,
                          const gfx::ColorSpace& color_space);
  void AllocateGpuMemoryBuffer(const gfx::Size& size,
                               viz::ResourceFormat format,
                               gfx::BufferUsage usage,
                               const gfx::ColorSpace& color_space);
  void Free();

  viz::ResourceTextureHint hint() const { return hint_; }

 private:
  LayerTreeResourceProvider* resource_provider_;
  viz::ResourceTextureHint hint_ = viz::ResourceTextureHint::kDefault;

#if DCHECK_IS_ON()
  base::PlatformThreadId allocate_thread_id_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedResource);
};

}  // namespace cc

#endif  // CC_RESOURCES_SCOPED_RESOURCE_H_
