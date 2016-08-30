// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_
#define BLIMP_CLIENT_PUBLIC_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_

#include "base/callback.h"
#include "base/memory/ptr_util.h"

namespace cc {
class ContextProvider;
class LayerTreeSettings;
class SurfaceManager;
}  // namespace cc

namespace gpu {
class GpuMemoryBufferManager;
}  // namespace gpu

namespace blimp {
namespace client {

// A set of compositor dependencies that must be provided by the embedder.  This
// is expected to outlive all BlimpContents instances.
class CompositorDependencies {
 public:
  using ContextProviderCallback =
      base::Callback<void(const scoped_refptr<cc::ContextProvider>&)>;

  virtual ~CompositorDependencies() = default;

  // Settings used to create all BlimpCompositor instances.
  virtual cc::LayerTreeSettings* GetLayerTreeSettings() = 0;
  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
  virtual cc::SurfaceManager* GetSurfaceManager() = 0;
  virtual uint32_t AllocateSurfaceId() = 0;

  // This call may return synchronously if the ContextProvider has already been
  // created.
  virtual void GetContextProvider(const ContextProviderCallback& callback) = 0;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_
