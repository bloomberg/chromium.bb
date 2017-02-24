// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/gpu_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/histograms.h"
#include "cc/paint/paint_canvas.h"
#include "cc/playback/image_hijack_canvas.h"
#include "cc/playback/raster_source.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/resources/resource.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {

static void RasterizeSource(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    const gfx::Size& resource_size,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings,
    ContextProvider* context_provider,
    ResourceProvider::ScopedWriteLockGL* resource_lock,
    bool async_worker_context_enabled,
    bool use_distance_field_text,
    int msaa_sample_count) {
  ScopedGpuRaster gpu_raster(context_provider);

  ResourceProvider::ScopedSkSurfaceProvider scoped_surface(
      context_provider, resource_lock, async_worker_context_enabled,
      use_distance_field_text, raster_source->CanUseLCDText(),
      msaa_sample_count);
  SkSurface* sk_surface = scoped_surface.sk_surface();
  // Allocating an SkSurface will fail after a lost context.  Pretend we
  // rasterized, as the contents of the resource don't matter anymore.
  if (!sk_surface) {
    DLOG(ERROR) << "Failed to allocate raster surface";
    return;
  }

  // Playback
  gfx::Rect playback_rect = raster_full_rect;
  if (resource_has_previous_content) {
    playback_rect.Intersect(raster_dirty_rect);
  }
  DCHECK(!playback_rect.IsEmpty())
      << "Why are we rastering a tile that's not dirty?";

  // Log a histogram of the percentage of pixels that were saved due to
  // partial raster.
  const char* client_name = GetClientNameForMetrics();
  float full_rect_size = raster_full_rect.size().GetArea();
  if (full_rect_size > 0 && client_name) {
    float fraction_partial_rastered =
        static_cast<float>(playback_rect.size().GetArea()) / full_rect_size;
    float fraction_saved = 1.0f - fraction_partial_rastered;
    UMA_HISTOGRAM_PERCENTAGE(
        base::StringPrintf("Renderer4.%s.PartialRasterPercentageSaved.Gpu",
                           client_name),
        100.0f * fraction_saved);
  }

  raster_source->PlaybackToCanvas(sk_surface->getCanvas(), raster_full_rect,
                                  playback_rect, scale, playback_settings);
}

}  // namespace

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    ResourceProvider* resource_provider,
    ResourceId resource_id,
    bool async_worker_context_enabled,
    bool resource_has_previous_content)
    : client_(client),
      lock_(resource_provider, resource_id, async_worker_context_enabled),
      resource_has_previous_content_(resource_has_previous_content) {
  client_->pending_raster_buffers_.insert(this);
}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  client_->pending_raster_buffers_.erase(this);
}

void GpuRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0("cc", "GpuRasterBuffer::Playback");
  client_->PlaybackOnWorkerThread(&lock_, sync_token_,
                                  resource_has_previous_content_, raster_source,
                                  raster_full_rect, raster_dirty_rect,
                                  new_content_id, scale, playback_settings);
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    ContextProvider* compositor_context_provider,
    ContextProvider* worker_context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text,
    int gpu_rasterization_msaa_sample_count,
    bool async_worker_context_enabled)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      msaa_sample_count_(gpu_rasterization_msaa_sample_count),
      async_worker_context_enabled_(async_worker_context_enabled) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() {
  DCHECK(pending_raster_buffers_.empty());
}

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return base::MakeUnique<RasterBufferImpl>(
      this, resource_provider_, resource->id(), async_worker_context_enabled_,
      resource_has_previous_content);
}

void GpuRasterBufferProvider::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void GpuRasterBufferProvider::OrderingBarrier() {
  TRACE_EVENT0("cc", "GpuRasterBufferProvider::OrderingBarrier");

  gpu::gles2::GLES2Interface* gl = compositor_context_provider_->ContextGL();
  if (async_worker_context_enabled_) {
    GLuint64 fence = gl->InsertFenceSyncCHROMIUM();
    gl->OrderingBarrierCHROMIUM();

    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence, sync_token.GetData());

    DCHECK(sync_token.HasData() ||
           gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);

    for (RasterBufferImpl* buffer : pending_raster_buffers_)
      buffer->set_sync_token(sync_token);
  } else {
    gl->OrderingBarrierCHROMIUM();
  }
  pending_raster_buffers_.clear();
}

ResourceFormat GpuRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  return resource_provider_->best_render_buffer_format();
}

bool GpuRasterBufferProvider::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  // This doesn't require a swizzle because we rasterize to the correct format.
  return false;
}

bool GpuRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  // Partial raster doesn't support MSAA, as the MSAA resolve is unaware of clip
  // rects.
  // TODO(crbug.com/629683): See if we can work around this limitation.
  return msaa_sample_count_ == 0;
}

bool GpuRasterBufferProvider::IsResourceReadyToDraw(
    ResourceId resource_id) const {
  if (!async_worker_context_enabled_)
    return true;

  gpu::SyncToken sync_token =
      resource_provider_->GetSyncTokenForResources({resource_id});
  if (!sync_token.HasData())
    return true;

  // IsSyncTokenSignalled is threadsafe, no need for worker context lock.
  return worker_context_provider_->ContextSupport()->IsSyncTokenSignalled(
      sync_token);
}

uint64_t GpuRasterBufferProvider::SetReadyToDrawCallback(
    const ResourceProvider::ResourceIdArray& resource_ids,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  if (!async_worker_context_enabled_)
    return 0;

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

void GpuRasterBufferProvider::Shutdown() {
  pending_raster_buffers_.clear();
}

void GpuRasterBufferProvider::PlaybackOnWorkerThread(
    ResourceProvider::ScopedWriteLockGL* resource_lock,
    const gpu::SyncToken& sync_token,
    bool resource_has_previous_content,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    float scale,
    const RasterSource::PlaybackSettings& playback_settings) {
  ContextProvider::ScopedContextLock scoped_context(worker_context_provider_);
  gpu::gles2::GLES2Interface* gl = scoped_context.ContextGL();
  DCHECK(gl);

  if (async_worker_context_enabled_) {
    // Early out if sync token is invalid. This happens if the compositor
    // context was lost before ScheduleTasks was called.
    if (!sync_token.HasData())
      return;
    // Synchronize with compositor.
    gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

  RasterizeSource(raster_source, resource_has_previous_content,
                  resource_lock->size(), raster_full_rect, raster_dirty_rect,
                  scale, playback_settings, worker_context_provider_,
                  resource_lock, async_worker_context_enabled_,
                  use_distance_field_text_, msaa_sample_count_);

  const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();

  // Barrier to sync worker context output to cc context.
  gl->OrderingBarrierCHROMIUM();

  // Generate sync token after the barrier for cross context synchronization.
  gpu::SyncToken resource_sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, resource_sync_token.GetData());
  resource_lock->set_sync_token(resource_sync_token);
  resource_lock->set_synchronized(!async_worker_context_enabled_);
}

}  // namespace cc
