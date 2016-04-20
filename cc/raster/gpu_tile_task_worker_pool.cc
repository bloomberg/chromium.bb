// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_tile_task_worker_pool.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/playback/raster_source.h"
#include "cc/raster/gpu_rasterizer.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/resources/resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(GpuRasterizer* rasterizer,
                   const Resource* resource,
                   uint64_t resource_content_id,
                   uint64_t previous_content_id)
      : rasterizer_(rasterizer),
        lock_(rasterizer->resource_provider(), resource->id()),
        resource_has_previous_content_(
            resource_content_id && resource_content_id == previous_content_id) {
  }

  // Overridden from RasterBuffer:
  void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings) override {
    TRACE_EVENT0("cc", "RasterBufferImpl::Playback");
    // GPU raster doesn't do low res tiles, so should always include images.
    DCHECK(!playback_settings.skip_images);
    ContextProvider* context_provider = rasterizer_->resource_provider()
                                            ->output_surface()
                                            ->worker_context_provider();
    DCHECK(context_provider);

    ContextProvider::ScopedContextLock scoped_context(context_provider);

    gfx::Rect playback_rect = raster_full_rect;
    if (resource_has_previous_content_) {
      playback_rect.Intersect(raster_dirty_rect);
    }
    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";

    // TODO(danakj): Implement partial raster with raster_dirty_rect.
    // Rasterize source into resource.
    rasterizer_->RasterizeSource(&lock_, raster_source, raster_full_rect,
                                 playback_rect, scale, playback_settings);

    gpu::gles2::GLES2Interface* gl = scoped_context.ContextGL();
    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();

    // Barrier to sync worker context output to cc context.
    gl->OrderingBarrierCHROMIUM();

    // Generate sync token after the barrier for cross context synchronization.
    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
    lock_.UpdateResourceSyncToken(sync_token);
  }

 private:
  GpuRasterizer* rasterizer_;
  ResourceProvider::ScopedWriteLockGr lock_;
  bool resource_has_previous_content_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace

// static
std::unique_ptr<TileTaskWorkerPool> GpuTileTaskWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text,
    int gpu_rasterization_msaa_sample_count) {
  return base::WrapUnique<TileTaskWorkerPool>(new GpuTileTaskWorkerPool(
      task_runner, task_graph_runner, context_provider, resource_provider,
      use_distance_field_text, gpu_rasterization_msaa_sample_count));
}

GpuTileTaskWorkerPool::GpuTileTaskWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text,
    int gpu_rasterization_msaa_sample_count)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner_->GetNamespaceToken()),
      rasterizer_(new GpuRasterizer(context_provider,
                                    resource_provider,
                                    use_distance_field_text,
                                    gpu_rasterization_msaa_sample_count)) {}

GpuTileTaskWorkerPool::~GpuTileTaskWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

void GpuTileTaskWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void GpuTileTaskWorkerPool::ScheduleTasks(TaskGraph* graph) {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::ScheduleTasks");

  ScheduleTasksOnOriginThread(this, graph);

  // Barrier to sync any new resources to the worker context.
  rasterizer_->resource_provider()
      ->output_surface()
      ->context_provider()
      ->ContextGL()
      ->OrderingBarrierCHROMIUM();

  task_graph_runner_->ScheduleTasks(namespace_token_, graph);
}

void GpuTileTaskWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  CompleteTasks(completed_tasks_);
  completed_tasks_.clear();
}

ResourceFormat GpuTileTaskWorkerPool::GetResourceFormat(
    bool must_support_alpha) const {
  return rasterizer_->resource_provider()->best_render_buffer_format();
}

bool GpuTileTaskWorkerPool::GetResourceRequiresSwizzle(
    bool must_support_alpha) const {
  // This doesn't require a swizzle because we rasterize to the correct format.
  return false;
}

RasterBufferProvider* GpuTileTaskWorkerPool::AsRasterBufferProvider() {
  return this;
}

void GpuTileTaskWorkerPool::CompleteTasks(const Task::Vector& tasks) {
  for (auto& task : tasks) {
    TileTask* tile_task = static_cast<TileTask*>(task.get());

    tile_task->WillComplete();
    tile_task->CompleteOnOriginThread(this);
    tile_task->DidComplete();
  }
  completed_tasks_.clear();
}

std::unique_ptr<RasterBuffer> GpuTileTaskWorkerPool::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  return std::unique_ptr<RasterBuffer>(new RasterBufferImpl(
      rasterizer_.get(), resource, resource_content_id, previous_content_id));
}

void GpuTileTaskWorkerPool::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

}  // namespace cc
