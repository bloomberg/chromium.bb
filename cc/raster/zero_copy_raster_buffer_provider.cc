// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/zero_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

constexpr static auto kBufferUsage = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

// Subclass for InUsePoolResource that holds ownership of a zero-copy backing
// and does cleanup of the backing when destroyed.
class ZeroCopyGpuBacking : public ResourcePool::GpuBacking {
 public:
  ~ZeroCopyGpuBacking() override {
    gpu::gles2::GLES2Interface* gl = compositor_context_provider->ContextGL();
    if (returned_sync_token.HasData())
      gl->WaitSyncTokenCHROMIUM(returned_sync_token.GetConstData());
    if (texture_id)
      gl->DeleteTextures(1, &texture_id);
    if (image_id)
      gl->DestroyImageCHROMIUM(image_id);
  }

  base::trace_event::MemoryAllocatorDumpGuid MemoryDumpGuid(
      uint64_t tracing_process_id) override {
    if (!gpu_memory_buffer)
      return {};
    return gpu_memory_buffer->GetGUIDForTracing(tracing_process_id);
  }

  base::UnguessableToken SharedMemoryGuid() override {
    if (!gpu_memory_buffer)
      return {};
    return gpu_memory_buffer->GetHandle().handle.GetGUID();
  }

  // The ContextProvider used to clean up the texture and image ids.
  viz::ContextProvider* compositor_context_provider = nullptr;
  // The backing for zero-copy gpu resources. The |texture_id| is bound to
  // this.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  // The texture id bound to the GpuMemoryBuffer.
  uint32_t texture_id = 0;
  // The image id that associates the |gpu_memory_buffer| and the
  // |texture_id|.
  uint32_t image_id = 0;
};

// RasterBuffer for the zero copy upload, which is given to the raster worker
// threads for raster/upload.
class ZeroCopyRasterBufferImpl : public RasterBuffer {
 public:
  ZeroCopyRasterBufferImpl(
      viz::ContextProvider* context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const ResourcePool::InUsePoolResource& in_use_resource,
      ZeroCopyGpuBacking* backing)
      : backing_(backing),
        gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
        resource_size_(in_use_resource.size()),
        resource_format_(in_use_resource.format()),
        resource_color_space_(in_use_resource.color_space()),
        gpu_memory_buffer_(std::move(backing_->gpu_memory_buffer)) {}

  ~ZeroCopyRasterBufferImpl() override {
    // This is destroyed on the compositor thread when raster is complete, but
    // before the backing is prepared for export to the display compositor. So
    // we can set up the texture and SyncToken here.
    // TODO(danakj): This could be done with the worker context in Playback. Do
    // we need to do things in IsResourceReadyToDraw() and OrderingBarrier then?
    gpu::gles2::GLES2Interface* gl =
        backing_->compositor_context_provider->ContextGL();
    const gpu::Capabilities& caps =
        backing_->compositor_context_provider->ContextCapabilities();

    if (backing_->returned_sync_token.HasData()) {
      gl->WaitSyncTokenCHROMIUM(backing_->returned_sync_token.GetConstData());
      backing_->returned_sync_token = gpu::SyncToken();
    }

    if (!backing_->texture_id) {
      // Make a texture and a mailbox for export of the GpuMemoryBuffer to the
      // display compositor.
      gl->GenTextures(1, &backing_->texture_id);
      backing_->texture_target = gpu::GetBufferTextureTarget(
          kBufferUsage, viz::BufferFormat(resource_format_), caps);
      backing_->mailbox = gpu::Mailbox::Generate();
      gl->ProduceTextureDirectCHROMIUM(backing_->texture_id,
                                       backing_->mailbox.name);
      backing_->overlay_candidate = true;
      // This RasterBufferProvider will modify the resource outside of the
      // GL command stream. So resources should not become available for reuse
      // until they are not in use by the gpu anymore, which a fence is used to
      // determine.
      backing_->wait_on_fence_required = true;

      gl->BindTexture(backing_->texture_target, backing_->texture_id);
      gl->TexParameteri(backing_->texture_target, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR);
      gl->TexParameteri(backing_->texture_target, GL_TEXTURE_MAG_FILTER,
                        GL_LINEAR);
      gl->TexParameteri(backing_->texture_target, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
      gl->TexParameteri(backing_->texture_target, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
    } else {
      gl->BindTexture(backing_->texture_target, backing_->texture_id);
    }

    if (!backing_->image_id) {
      // If GpuMemoryBuffer allocation failed (https://crbug.com/554541), then
      // we don't have anything to give to the display compositor, but also no
      // way to report an error, so we just make a texture but don't bind
      // anything to it..
      if (gpu_memory_buffer_) {
        backing_->image_id = gl->CreateImageCHROMIUM(
            gpu_memory_buffer_->AsClientBuffer(), resource_size_.width(),
            resource_size_.height(), viz::GLInternalFormat(resource_format_));
        gl->BindTexImage2DCHROMIUM(backing_->texture_target,
                                   backing_->image_id);
      }
    } else {
      gl->ReleaseTexImage2DCHROMIUM(backing_->texture_target,
                                    backing_->image_id);
      gl->BindTexImage2DCHROMIUM(backing_->texture_target, backing_->image_id);
    }
    gl->BindTexture(backing_->texture_target, 0);

    backing_->mailbox_sync_token =
        LayerTreeResourceProvider::GenerateSyncTokenHelper(gl);
    backing_->gpu_memory_buffer = std::move(gpu_memory_buffer_);
  }

  // Overridden from RasterBuffer:
  void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings) override {
    TRACE_EVENT0("cc", "ZeroCopyRasterBuffer::Playback");

    if (!gpu_memory_buffer_) {
      gpu_memory_buffer_ = gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          resource_size_, viz::BufferFormat(resource_format_), kBufferUsage,
          gpu::kNullSurfaceHandle);
      // GpuMemoryBuffer allocation can fail (https://crbug.com/554541).
      if (!gpu_memory_buffer_)
        return;
      gpu_memory_buffer_->SetColorSpace(resource_color_space_);
    }

    DCHECK_EQ(1u, gfx::NumberOfPlanesForBufferFormat(
                      gpu_memory_buffer_->GetFormat()));
    bool rv = gpu_memory_buffer_->Map();
    DCHECK(rv);
    DCHECK(gpu_memory_buffer_->memory(0));
    // RasterBufferProvider::PlaybackToMemory only supports unsigned strides.
    DCHECK_GE(gpu_memory_buffer_->stride(0), 0);

    // TODO(danakj): Implement partial raster with raster_dirty_rect.
    RasterBufferProvider::PlaybackToMemory(
        gpu_memory_buffer_->memory(0), resource_format_, resource_size_,
        gpu_memory_buffer_->stride(0), raster_source, raster_full_rect,
        raster_full_rect, transform, resource_color_space_, playback_settings);
    gpu_memory_buffer_->Unmap();
  }

 private:
  // This field may only be used on the compositor thread.
  ZeroCopyGpuBacking* backing_;

  // These fields are for use on the worker thread.
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  gfx::Size resource_size_;
  viz::ResourceFormat resource_format_;
  gfx::ColorSpace resource_color_space_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ZeroCopyRasterBufferImpl);
};

}  // namespace

ZeroCopyRasterBufferProvider::ZeroCopyRasterBufferProvider(
    LayerTreeResourceProvider* resource_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    viz::ContextProvider* compositor_context_provider,
    viz::ResourceFormat preferred_tile_format)
    : resource_provider_(resource_provider),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      compositor_context_provider_(compositor_context_provider),
      preferred_tile_format_(preferred_tile_format) {}

ZeroCopyRasterBufferProvider::~ZeroCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
ZeroCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  if (!resource.gpu_backing()) {
    auto backing = std::make_unique<ZeroCopyGpuBacking>();
    backing->compositor_context_provider = compositor_context_provider_;
    resource.set_gpu_backing(std::move(backing));
  }
  ZeroCopyGpuBacking* backing =
      static_cast<ZeroCopyGpuBacking*>(resource.gpu_backing());

  return std::make_unique<ZeroCopyRasterBufferImpl>(
      compositor_context_provider_, gpu_memory_buffer_manager_, resource,
      backing);
}

void ZeroCopyRasterBufferProvider::Flush() {}

viz::ResourceFormat ZeroCopyRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  if (resource_provider_->IsTextureFormatSupported(preferred_tile_format_)) {
    if (!must_support_alpha)
      return preferred_tile_format_;
    if (DoesResourceFormatSupportAlpha(preferred_tile_format_))
      return preferred_tile_format_;
  }
  return resource_provider_->best_texture_format();
}

bool ZeroCopyRasterBufferProvider::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

bool ZeroCopyRasterBufferProvider::IsResourcePremultiplied(
    bool must_support_alpha) const {
  return true;
}

bool ZeroCopyRasterBufferProvider::CanPartialRasterIntoProvidedResource()
    const {
  return false;
}

bool ZeroCopyRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  // Zero-copy resources are immediately ready to draw.
  return true;
}

uint64_t ZeroCopyRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  // Zero-copy resources are immediately ready to draw.
  return 0;
}

void ZeroCopyRasterBufferProvider::Shutdown() {}

}  // namespace cc
