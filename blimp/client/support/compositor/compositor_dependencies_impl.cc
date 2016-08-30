// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/support/compositor/compositor_dependencies_impl.h"

#include "blimp/client/support/compositor/blimp_context_provider.h"
#include "blimp/client/support/compositor/blimp_gpu_memory_buffer_manager.h"
#include "blimp/client/support/compositor/blimp_layer_tree_settings.h"
#include "cc/output/context_provider.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_settings.h"

#if defined(OS_ANDROID)
#include "ui/gfx/android/device_display_info.h"
#endif

namespace blimp {
namespace client {

CompositorDependenciesImpl::CompositorDependenciesImpl()
    : gpu_memory_buffer_manager_(
          base::MakeUnique<BlimpGpuMemoryBufferManager>()),
      surface_manager_(base::MakeUnique<cc::SurfaceManager>()),
      next_surface_id_(0) {}

CompositorDependenciesImpl::~CompositorDependenciesImpl() = default;

cc::LayerTreeSettings* CompositorDependenciesImpl::GetLayerTreeSettings() {
  if (!settings_) {
    settings_ = base::MakeUnique<cc::LayerTreeSettings>();

    PopulateCommonLayerTreeSettings(settings_.get());
    settings_->abort_commit_before_output_surface_creation = false;
    settings_->renderer_settings.buffer_to_texture_target_map =
        BlimpGpuMemoryBufferManager::GetDefaultBufferToTextureTargetMap();
    settings_->use_output_surface_begin_frame_source = true;

    int default_tile_size = 256;
#if defined(OS_ANDROID)
    gfx::DeviceDisplayInfo info;
    bool real_size_supported = true;
    int display_width = info.GetPhysicalDisplayWidth();
    int display_height = info.GetPhysicalDisplayHeight();
    if (display_width == 0 || display_height == 0) {
      real_size_supported = false;
      display_width = info.GetDisplayWidth();
      display_height = info.GetDisplayHeight();
    }

    int portrait_width = std::min(display_width, display_height);
    int landscape_width = std::max(display_width, display_height);

    if (real_size_supported) {
      // Maximum HD dimensions should be 768x1280
      // Maximum FHD dimensions should be 1200x1920
      if (portrait_width > 768 || landscape_width > 1280)
        default_tile_size = 384;
      if (portrait_width > 1200 || landscape_width > 1920)
        default_tile_size = 512;

      // Adjust for some resolutions that barely straddle an extra
      // tile when in portrait mode. This helps worst case scroll/raster
      // by not needing a full extra tile for each row.
      if (default_tile_size == 256 && portrait_width == 768)
        default_tile_size += 32;
      if (default_tile_size == 384 && portrait_width == 1200)
        default_tile_size += 32;
    } else {
      // We don't know the exact resolution due to screen controls etc.
      // So this just estimates the values above using tile counts.
      int numTiles = (display_width * display_height) / (256 * 256);
      if (numTiles > 16)
        default_tile_size = 384;
      if (numTiles >= 40)
        default_tile_size = 512;
    }
#endif
    settings_->default_tile_size.SetSize(default_tile_size, default_tile_size);
  }

  return settings_.get();
}

gpu::GpuMemoryBufferManager*
CompositorDependenciesImpl::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_.get();
}

cc::SurfaceManager* CompositorDependenciesImpl::GetSurfaceManager() {
  return surface_manager_.get();
}

uint32_t CompositorDependenciesImpl::AllocateSurfaceId() {
  return ++next_surface_id_;
}

void CompositorDependenciesImpl::GetContextProvider(
    const CompositorDependencies::ContextProviderCallback& callback) {
  scoped_refptr<cc::ContextProvider> provider = BlimpContextProvider::Create(
      gfx::kNullAcceleratedWidget, gpu_memory_buffer_manager_.get());
  callback.Run(provider);
}

}  // namespace client
}  // namespace blimp
