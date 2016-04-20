// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/one_copy_tile_task_worker_pool.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/macros.h"
#include "cc/base/math_util.h"
#include "cc/raster/staging_buffer_pool.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/gfx/buffer_format_util.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(OneCopyTileTaskWorkerPool* worker_pool,
                   ResourceProvider* resource_provider,
                   ResourceFormat resource_format,
                   const Resource* resource,
                   uint64_t previous_content_id)
      : worker_pool_(worker_pool),
        resource_(resource),
        lock_(resource_provider, resource->id()),
        previous_content_id_(previous_content_id) {}

  ~RasterBufferImpl() override {}

  // Overridden from RasterBuffer:
  void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings) override {
    worker_pool_->PlaybackAndCopyOnWorkerThread(
        resource_, &lock_, raster_source, raster_full_rect, raster_dirty_rect,
        scale, playback_settings, previous_content_id_, new_content_id);
  }

 private:
  OneCopyTileTaskWorkerPool* worker_pool_;
  const Resource* resource_;
  ResourceProvider::ScopedWriteLockGL lock_;
  uint64_t previous_content_id_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

// 4MiB is the size of 4 512x512 tiles, which has proven to be a good
// default batch size for copy operations.
const int kMaxBytesPerCopyOperation = 1024 * 1024 * 4;

}  // namespace

// static
std::unique_ptr<TileTaskWorkerPool> OneCopyTileTaskWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    int max_copy_texture_chromium_size,
    bool use_partial_raster,
    int max_staging_buffer_usage_in_bytes,
    ResourceFormat preferred_tile_format) {
  return base::WrapUnique<TileTaskWorkerPool>(new OneCopyTileTaskWorkerPool(
      task_runner, task_graph_runner, resource_provider,
      max_copy_texture_chromium_size, use_partial_raster,
      max_staging_buffer_usage_in_bytes, preferred_tile_format));
}

OneCopyTileTaskWorkerPool::OneCopyTileTaskWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    int max_copy_texture_chromium_size,
    bool use_partial_raster,
    int max_staging_buffer_usage_in_bytes,
    ResourceFormat preferred_tile_format)
    : task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      resource_provider_(resource_provider),
      max_bytes_per_copy_operation_(
          max_copy_texture_chromium_size
              ? std::min(kMaxBytesPerCopyOperation,
                         max_copy_texture_chromium_size)
              : kMaxBytesPerCopyOperation),
      use_partial_raster_(use_partial_raster),
      bytes_scheduled_since_last_flush_(0),
      preferred_tile_format_(preferred_tile_format) {
  staging_pool_ = StagingBufferPool::Create(task_runner, resource_provider,
                                            use_partial_raster,
                                            max_staging_buffer_usage_in_bytes);
}

OneCopyTileTaskWorkerPool::~OneCopyTileTaskWorkerPool() {
}

void OneCopyTileTaskWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "OneCopyTileTaskWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);

  staging_pool_->Shutdown();
}

void OneCopyTileTaskWorkerPool::ScheduleTasks(TaskGraph* graph) {
  TRACE_EVENT0("cc", "OneCopyTileTaskWorkerPool::ScheduleTasks");

  ScheduleTasksOnOriginThread(this, graph);

  // Barrier to sync any new resources to the worker context.
  resource_provider_->output_surface()
      ->context_provider()
      ->ContextGL()
      ->OrderingBarrierCHROMIUM();

  task_graph_runner_->ScheduleTasks(namespace_token_, graph);
}

void OneCopyTileTaskWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "OneCopyTileTaskWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);

  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end(); ++it) {
    TileTask* task = static_cast<TileTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();
  }
  completed_tasks_.clear();
}

ResourceFormat OneCopyTileTaskWorkerPool::GetResourceFormat(
    bool must_support_alpha) const {
  if (resource_provider_->IsResourceFormatSupported(preferred_tile_format_) &&
      (DoesResourceFormatSupportAlpha(preferred_tile_format_) ||
       !must_support_alpha)) {
    return preferred_tile_format_;
  }

  return resource_provider_->best_texture_format();
}

bool OneCopyTileTaskWorkerPool::GetResourceRequiresSwizzle(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

RasterBufferProvider* OneCopyTileTaskWorkerPool::AsRasterBufferProvider() {
  return this;
}

std::unique_ptr<RasterBuffer> OneCopyTileTaskWorkerPool::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  // TODO(danakj): If resource_content_id != 0, we only need to copy/upload
  // the dirty rect.
  return base::WrapUnique<RasterBuffer>(
      new RasterBufferImpl(this, resource_provider_, resource->format(),
                           resource, previous_content_id));
}

void OneCopyTileTaskWorkerPool::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void OneCopyTileTaskWorkerPool::PlaybackAndCopyOnWorkerThread(
    const Resource* resource,
    ResourceProvider::ScopedWriteLockGL* resource_lock,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  std::unique_ptr<StagingBuffer> staging_buffer =
      staging_pool_->AcquireStagingBuffer(resource, previous_content_id);

  PlaybackToStagingBuffer(staging_buffer.get(), resource, raster_source,
                          raster_full_rect, raster_dirty_rect, scale,
                          playback_settings, previous_content_id,
                          new_content_id);

  CopyOnWorkerThread(staging_buffer.get(), resource, resource_lock,
                     raster_source, previous_content_id, new_content_id);

  staging_pool_->ReleaseStagingBuffer(std::move(staging_buffer));
}

void OneCopyTileTaskWorkerPool::PlaybackToStagingBuffer(
    StagingBuffer* staging_buffer,
    const Resource* resource,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  // Allocate GpuMemoryBuffer if necessary. If using partial raster, we
  // must allocate a buffer with BufferUsage CPU_READ_WRITE_PERSISTENT.
  if (!staging_buffer->gpu_memory_buffer) {
    staging_buffer->gpu_memory_buffer =
        resource_provider_->gpu_memory_buffer_manager()
            ->AllocateGpuMemoryBuffer(
                staging_buffer->size, BufferFormat(resource->format()),
                use_partial_raster_
                    ? gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT
                    : gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                0 /* surface_id */);
  }

  gfx::Rect playback_rect = raster_full_rect;
  if (use_partial_raster_ && previous_content_id) {
    // Reduce playback rect to dirty region if the content id of the staging
    // buffer matches the prevous content id.
    if (previous_content_id == staging_buffer->content_id)
      playback_rect.Intersect(raster_dirty_rect);
  }

  if (staging_buffer->gpu_memory_buffer) {
    gfx::GpuMemoryBuffer* buffer = staging_buffer->gpu_memory_buffer.get();
    DCHECK_EQ(1u, gfx::NumberOfPlanesForBufferFormat(buffer->GetFormat()));
    bool rv = buffer->Map();
    DCHECK(rv);
    DCHECK(buffer->memory(0));
    // TileTaskWorkerPool::PlaybackToMemory only supports unsigned strides.
    DCHECK_GE(buffer->stride(0), 0);

    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";
    TileTaskWorkerPool::PlaybackToMemory(
        buffer->memory(0), resource->format(), staging_buffer->size,
        buffer->stride(0), raster_source, raster_full_rect, playback_rect,
        scale, playback_settings);
    buffer->Unmap();
    staging_buffer->content_id = new_content_id;
  }
}

void OneCopyTileTaskWorkerPool::CopyOnWorkerThread(
    StagingBuffer* staging_buffer,
    const Resource* resource,
    ResourceProvider::ScopedWriteLockGL* resource_lock,
    const RasterSource* raster_source,
    uint64_t previous_content_id,
    uint64_t new_content_id) {
  ContextProvider* context_provider =
      resource_provider_->output_surface()->worker_context_provider();
  DCHECK(context_provider);

  {
    ContextProvider::ScopedContextLock scoped_context(context_provider);

    gpu::gles2::GLES2Interface* gl = scoped_context.ContextGL();
    DCHECK(gl);

    unsigned image_target =
        resource_provider_->GetImageTextureTarget(resource->format());

    // Create and bind staging texture.
    if (!staging_buffer->texture_id) {
      gl->GenTextures(1, &staging_buffer->texture_id);
      gl->BindTexture(image_target, staging_buffer->texture_id);
      gl->TexParameteri(image_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      gl->TexParameteri(image_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      gl->TexParameteri(image_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gl->TexParameteri(image_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
      gl->BindTexture(image_target, staging_buffer->texture_id);
    }

    // Create and bind image.
    if (!staging_buffer->image_id) {
      if (staging_buffer->gpu_memory_buffer) {
        staging_buffer->image_id = gl->CreateImageCHROMIUM(
            staging_buffer->gpu_memory_buffer->AsClientBuffer(),
            staging_buffer->size.width(), staging_buffer->size.height(),
            GLInternalFormat(resource->format()));
        gl->BindTexImage2DCHROMIUM(image_target, staging_buffer->image_id);
      }
    } else {
      gl->ReleaseTexImage2DCHROMIUM(image_target, staging_buffer->image_id);
      gl->BindTexImage2DCHROMIUM(image_target, staging_buffer->image_id);
    }

    // Unbind staging texture.
    gl->BindTexture(image_target, 0);

    if (resource_provider_->use_sync_query()) {
      if (!staging_buffer->query_id)
        gl->GenQueriesEXT(1, &staging_buffer->query_id);

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
      // TODO(reveman): This avoids a performance problem on ARM ChromeOS
      // devices. crbug.com/580166
      gl->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, staging_buffer->query_id);
#else
      gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM,
                        staging_buffer->query_id);
#endif
    }

    // Since compressed texture's cannot be pre-allocated we might have an
    // unallocated resource in which case we need to perform a full size copy.
    if (IsResourceFormatCompressed(resource->format())) {
      gl->CompressedCopyTextureCHROMIUM(staging_buffer->texture_id,
                                        resource_lock->texture_id());
    } else {
      int bytes_per_row = ResourceUtil::UncheckedWidthInBytes<int>(
          resource->size().width(), resource->format());
      int chunk_size_in_rows =
          std::max(1, max_bytes_per_copy_operation_ / bytes_per_row);
      // Align chunk size to 4. Required to support compressed texture formats.
      chunk_size_in_rows = MathUtil::UncheckedRoundUp(chunk_size_in_rows, 4);
      int y = 0;
      int height = resource->size().height();
      while (y < height) {
        // Copy at most |chunk_size_in_rows|.
        int rows_to_copy = std::min(chunk_size_in_rows, height - y);
        DCHECK_GT(rows_to_copy, 0);

        gl->CopySubTextureCHROMIUM(
            staging_buffer->texture_id, resource_lock->texture_id(), 0, y, 0, y,
            resource->size().width(), rows_to_copy, false, false, false);
        y += rows_to_copy;

        // Increment |bytes_scheduled_since_last_flush_| by the amount of memory
        // used for this copy operation.
        bytes_scheduled_since_last_flush_ += rows_to_copy * bytes_per_row;

        if (bytes_scheduled_since_last_flush_ >=
            max_bytes_per_copy_operation_) {
          gl->ShallowFlushCHROMIUM();
          bytes_scheduled_since_last_flush_ = 0;
        }
      }
    }

    if (resource_provider_->use_sync_query()) {
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
      gl->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
#else
      gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
#endif
    }

    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();

    // Barrier to sync worker context output to cc context.
    gl->OrderingBarrierCHROMIUM();

    // Generate sync token after the barrier for cross context synchronization.
    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
    resource_lock->UpdateResourceSyncToken(sync_token);
  }
}

}  // namespace cc
