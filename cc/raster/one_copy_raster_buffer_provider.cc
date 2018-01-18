// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/one_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "ui/gfx/buffer_format_util.h"

namespace cc {
namespace {

// 4MiB is the size of 4 512x512 tiles, which has proven to be a good
// default batch size for copy operations.
const int kMaxBytesPerCopyOperation = 1024 * 1024 * 4;

}  // namespace

OneCopyRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    OneCopyRasterBufferProvider* client,
    LayerTreeResourceProvider* resource_provider,
    const ResourcePool::InUsePoolResource& in_use_resource,
    uint64_t previous_content_id)
    : client_(client),
      resource_size_(in_use_resource.size()),
      resource_format_(in_use_resource.format()),
      lock_(resource_provider, in_use_resource.gpu_backing_resource_id()),
      previous_content_id_(previous_content_id) {
  client_->pending_raster_buffers_.insert(this);
  lock_.CreateMailbox();
}

OneCopyRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  client_->pending_raster_buffers_.erase(this);
}

void OneCopyRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0("cc", "OneCopyRasterBuffer::Playback");
  client_->PlaybackAndCopyOnWorkerThread(
      &lock_, sync_token_, raster_source, raster_full_rect, raster_dirty_rect,
      transform, resource_size_, resource_format_, playback_settings,
      previous_content_id_, new_content_id);
}

OneCopyRasterBufferProvider::OneCopyRasterBufferProvider(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    viz::ContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    LayerTreeResourceProvider* resource_provider,
    int max_copy_texture_chromium_size,
    bool use_partial_raster,
    int max_staging_buffer_usage_in_bytes,
    viz::ResourceFormat preferred_tile_format)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      resource_provider_(resource_provider),
      max_bytes_per_copy_operation_(
          max_copy_texture_chromium_size
              ? std::min(kMaxBytesPerCopyOperation,
                         max_copy_texture_chromium_size)
              : kMaxBytesPerCopyOperation),
      use_partial_raster_(use_partial_raster),
      bytes_scheduled_since_last_flush_(0),
      preferred_tile_format_(preferred_tile_format),
      staging_pool_(std::move(task_runner),
                    worker_context_provider,
                    resource_provider,
                    use_partial_raster,
                    max_staging_buffer_usage_in_bytes) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

OneCopyRasterBufferProvider::~OneCopyRasterBufferProvider() {
  DCHECK(pending_raster_buffers_.empty());
}

std::unique_ptr<RasterBuffer>
OneCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  // TODO(danakj): If resource_content_id != 0, we only need to copy/upload
  // the dirty rect.
  return std::make_unique<RasterBufferImpl>(this, resource_provider_, resource,
                                            previous_content_id);
}

void OneCopyRasterBufferProvider::OrderingBarrier() {
  TRACE_EVENT0("cc", "OneCopyRasterBufferProvider::OrderingBarrier");

  gpu::gles2::GLES2Interface* gl = compositor_context_provider_->ContextGL();
  gpu::SyncToken sync_token =
      LayerTreeResourceProvider::GenerateSyncTokenHelper(gl);
  for (RasterBufferImpl* buffer : pending_raster_buffers_)
    buffer->set_sync_token(sync_token);
  pending_raster_buffers_.clear();
}

void OneCopyRasterBufferProvider::Flush() {
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

viz::ResourceFormat OneCopyRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  if (resource_provider_->IsTextureFormatSupported(preferred_tile_format_) &&
      (DoesResourceFormatSupportAlpha(preferred_tile_format_) ||
       !must_support_alpha)) {
    return preferred_tile_format_;
  }

  return resource_provider_->best_texture_format();
}

bool OneCopyRasterBufferProvider::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

bool OneCopyRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  // While OneCopyRasterBufferProvider has an internal partial raster
  // implementation, it cannot directly partial raster into the externally
  // owned resource provided in AcquireBufferForRaster.
  return false;
}

bool OneCopyRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  gpu::SyncToken sync_token = resource_provider_->GetSyncTokenForResources(
      {resource.gpu_backing_resource_id()});
  if (!sync_token.HasData())
    return true;

  // IsSyncTokenSignaled is thread-safe, no need for worker context lock.
  return worker_context_provider_->ContextSupport()->IsSyncTokenSignaled(
      sync_token);
}

uint64_t OneCopyRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  std::vector<viz::ResourceId> resource_ids;
  resource_ids.reserve(resources.size());
  for (auto* resource : resources)
    resource_ids.push_back(resource->gpu_backing_resource_id());
  gpu::SyncToken sync_token =
      resource_provider_->GetSyncTokenForResources(resource_ids);
  uint64_t callback_id = sync_token.release_count();
  DCHECK_NE(callback_id, 0u);

  // If the callback is different from the one the caller is already waiting on,
  // pass the callback through to SignalSyncToken. Otherwise the request is
  // redundant.
  if (callback_id != pending_callback_id) {
    // Use the compositor context because we want this callback on the impl
    // thread.
    compositor_context_provider_->ContextSupport()->SignalSyncToken(sync_token,
                                                                    callback);
  }

  return callback_id;
}

void OneCopyRasterBufferProvider::Shutdown() {
  staging_pool_.Shutdown();
  pending_raster_buffers_.clear();
}

void OneCopyRasterBufferProvider::PlaybackAndCopyOnWorkerThread(
    LayerTreeResourceProvider::ScopedWriteLockRaster* resource_lock,
    const gpu::SyncToken& sync_token,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    const gfx::AxisTransform2d& transform,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  WaitSyncToken(sync_token);

  std::unique_ptr<StagingBuffer> staging_buffer =
      staging_pool_.AcquireStagingBuffer(resource_size, resource_format,
                                         previous_content_id);

  PlaybackToStagingBuffer(
      staging_buffer.get(), raster_source, raster_full_rect, raster_dirty_rect,
      transform, resource_format, resource_lock->color_space_for_raster(),
      playback_settings, previous_content_id, new_content_id);

  CopyOnWorkerThread(staging_buffer.get(), resource_lock, raster_source,
                     raster_full_rect);

  staging_pool_.ReleaseStagingBuffer(std::move(staging_buffer));
}

void OneCopyRasterBufferProvider::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);
  // Synchronize with compositor. Nop if sync token is empty.
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

void OneCopyRasterBufferProvider::PlaybackToStagingBuffer(
    StagingBuffer* staging_buffer,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    const gfx::AxisTransform2d& transform,
    viz::ResourceFormat format,
    const gfx::ColorSpace& dst_color_space,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  // Allocate GpuMemoryBuffer if necessary. If using partial raster, we
  // must allocate a buffer with BufferUsage CPU_READ_WRITE_PERSISTENT.
  if (!staging_buffer->gpu_memory_buffer) {
    staging_buffer->gpu_memory_buffer =
        resource_provider_->gpu_memory_buffer_manager()->CreateGpuMemoryBuffer(
            staging_buffer->size, BufferFormat(format), StagingBufferUsage(),
            gpu::kNullSurfaceHandle);
  }

  gfx::Rect playback_rect = raster_full_rect;
  if (use_partial_raster_ && previous_content_id) {
    // Reduce playback rect to dirty region if the content id of the staging
    // buffer matches the prevous content id.
    if (previous_content_id == staging_buffer->content_id)
      playback_rect.Intersect(raster_dirty_rect);
  }

  // Log a histogram of the percentage of pixels that were saved due to
  // partial raster.
  const char* client_name = GetClientNameForMetrics();
  float full_rect_size = raster_full_rect.size().GetArea();
  if (full_rect_size > 0 && client_name) {
    float fraction_partial_rastered =
        static_cast<float>(playback_rect.size().GetArea()) / full_rect_size;
    float fraction_saved = 1.0f - fraction_partial_rastered;
    UMA_HISTOGRAM_PERCENTAGE(
        base::StringPrintf("Renderer4.%s.PartialRasterPercentageSaved.OneCopy",
                           client_name),
        100.0f * fraction_saved);
  }

  if (staging_buffer->gpu_memory_buffer) {
    gfx::GpuMemoryBuffer* buffer = staging_buffer->gpu_memory_buffer.get();
    DCHECK_EQ(1u, gfx::NumberOfPlanesForBufferFormat(buffer->GetFormat()));
    bool rv = buffer->Map();
    DCHECK(rv);
    DCHECK(buffer->memory(0));
    // RasterBufferProvider::PlaybackToMemory only supports unsigned strides.
    DCHECK_GE(buffer->stride(0), 0);

    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";
    RasterBufferProvider::PlaybackToMemory(
        buffer->memory(0), format, staging_buffer->size, buffer->stride(0),
        raster_source, raster_full_rect, playback_rect, transform,
        dst_color_space, playback_settings);
    buffer->Unmap();
    staging_buffer->content_id = new_content_id;
  }
}

void OneCopyRasterBufferProvider::CopyOnWorkerThread(
    StagingBuffer* staging_buffer,
    LayerTreeResourceProvider::ScopedWriteLockRaster* resource_lock,
    const RasterSource* raster_source,
    const gfx::Rect& rect_to_copy) {
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);

  GLuint texture_id = resource_lock->ConsumeTexture(ri);

  GLenum image_target = resource_provider_->GetImageTextureTarget(
      worker_context_provider_->ContextCapabilities(), StagingBufferUsage(),
      staging_buffer->format);

  // Create and bind staging texture.
  if (!staging_buffer->texture_id) {
    ri->GenTextures(1, &staging_buffer->texture_id);
    ri->BindTexture(image_target, staging_buffer->texture_id);
    ri->TexParameteri(image_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ri->TexParameteri(image_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ri->TexParameteri(image_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ri->TexParameteri(image_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    ri->BindTexture(image_target, staging_buffer->texture_id);
  }

  // Create and bind image.
  if (!staging_buffer->image_id) {
    if (staging_buffer->gpu_memory_buffer) {
      staging_buffer->image_id = ri->CreateImageCHROMIUM(
          staging_buffer->gpu_memory_buffer->AsClientBuffer(),
          staging_buffer->size.width(), staging_buffer->size.height(),
          GLInternalFormat(resource_lock->format()));
      ri->BindTexImage2DCHROMIUM(image_target, staging_buffer->image_id);
    }
  } else {
    ri->ReleaseTexImage2DCHROMIUM(image_target, staging_buffer->image_id);
    ri->BindTexImage2DCHROMIUM(image_target, staging_buffer->image_id);
  }

  // Unbind staging texture.
  ri->BindTexture(image_target, 0);

  if (resource_provider_->use_sync_query()) {
    if (!staging_buffer->query_id)
      ri->GenQueriesEXT(1, &staging_buffer->query_id);

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
    // TODO(reveman): This avoids a performance problem on ARM ChromeOS
    // devices. crbug.com/580166
    ri->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, staging_buffer->query_id);
#else
    ri->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, staging_buffer->query_id);
#endif
  }

  // Since compressed texture's cannot be pre-allocated we might have an
  // unallocated resource in which case we need to perform a full size copy.
  if (IsResourceFormatCompressed(resource_lock->format())) {
    ri->CompressedCopyTextureCHROMIUM(staging_buffer->texture_id, texture_id);
  } else {
    int bytes_per_row = ResourceUtil::UncheckedWidthInBytes<int>(
        rect_to_copy.width(), resource_lock->format());
    int chunk_size_in_rows =
        std::max(1, max_bytes_per_copy_operation_ / bytes_per_row);
    // Align chunk size to 4. Required to support compressed texture formats.
    chunk_size_in_rows = MathUtil::UncheckedRoundUp(chunk_size_in_rows, 4);
    int y = 0;
    int height = rect_to_copy.height();
    while (y < height) {
      // Copy at most |chunk_size_in_rows|.
      int rows_to_copy = std::min(chunk_size_in_rows, height - y);
      DCHECK_GT(rows_to_copy, 0);

      ri->CopySubTextureCHROMIUM(
          staging_buffer->texture_id, 0, GL_TEXTURE_2D, texture_id, 0, 0, y, 0,
          y, rect_to_copy.width(), rows_to_copy, false, false, false);
      y += rows_to_copy;

      // Increment |bytes_scheduled_since_last_flush_| by the amount of memory
      // used for this copy operation.
      bytes_scheduled_since_last_flush_ += rows_to_copy * bytes_per_row;

      if (bytes_scheduled_since_last_flush_ >= max_bytes_per_copy_operation_) {
        ri->ShallowFlushCHROMIUM();
        bytes_scheduled_since_last_flush_ = 0;
      }
    }
  }

  if (resource_provider_->use_sync_query()) {
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
    ri->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
#else
    ri->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
#endif
  }

  ri->DeleteTextures(1, &texture_id);

  // Generate sync token for cross context synchronization.
  resource_lock->set_sync_token(
      LayerTreeResourceProvider::GenerateSyncTokenHelper(ri));
}

gfx::BufferUsage OneCopyRasterBufferProvider::StagingBufferUsage() const {
  return use_partial_raster_
             ? gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT
             : gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
}

}  // namespace cc
