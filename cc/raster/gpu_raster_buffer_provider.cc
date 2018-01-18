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
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_recorder.h"
#include "cc/raster/raster_source.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "cc/resources/resource.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/axis_transform2d.h"

namespace cc {
namespace {

static void RasterizeSourceOOP(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    const gfx::Size& resource_size,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    viz::RasterContextProvider* context_provider,
    LayerTreeResourceProvider::ScopedWriteLockRaster* resource_lock,
    bool use_distance_field_text,
    int msaa_sample_count) {
  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  GLuint texture_id = resource_lock->ConsumeTexture(ri);

  ri->BeginRasterCHROMIUM(texture_id, raster_source->background_color(),
                          msaa_sample_count, playback_settings.use_lcd_text,
                          use_distance_field_text,
                          resource_lock->PixelConfig());
  // TODO(enne): need to pass color space into this function as well.
  float recording_to_raster_scale =
      transform.scale() / raster_source->recording_scale_factor();
  ri->RasterCHROMIUM(raster_source->GetDisplayItemList().get(),
                     playback_settings.image_provider,
                     raster_full_rect.OffsetFromOrigin(), playback_rect,
                     transform.translation(), recording_to_raster_scale);
  ri->EndRasterCHROMIUM();

  ri->DeleteTextures(1, &texture_id);
}

// The following class is needed to correctly reset GL state when rendering to
// SkCanvases with a GrContext on a RasterInterface enabled context.
class ScopedGrContextAccess {
 public:
  explicit ScopedGrContextAccess(viz::RasterContextProvider* context_provider)
      : context_provider_(context_provider) {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->BeginGpuRaster();

    class GrContext* gr_context = context_provider_->GrContext();
    gr_context->resetContext();
  }
  ~ScopedGrContextAccess() {
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    ri->EndGpuRaster();
  }

 private:
  viz::RasterContextProvider* context_provider_;
};

static void RasterizeSource(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    const gfx::Size& resource_size,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    viz::RasterContextProvider* context_provider,
    LayerTreeResourceProvider::ScopedWriteLockRaster* resource_lock,
    bool use_distance_field_text,
    int msaa_sample_count) {
  ScopedGrContextAccess gr_context_access(context_provider);

  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  GLuint texture_id = resource_lock->ConsumeTexture(ri);

  {
    LayerTreeResourceProvider::ScopedSkSurface scoped_surface(
        context_provider->GrContext(), texture_id, resource_lock->target(),
        resource_lock->size(), resource_lock->format(), use_distance_field_text,
        playback_settings.use_lcd_text, msaa_sample_count);

    SkSurface* surface = scoped_surface.surface();

    // Allocating an SkSurface will fail after a lost context.  Pretend we
    // rasterized, as the contents of the resource don't matter anymore.
    if (!surface) {
      DLOG(ERROR) << "Failed to allocate raster surface";
      return;
    }

    SkCanvas* canvas = surface->getCanvas();

    // As an optimization, inform Skia to discard when not doing partial raster.
    if (raster_full_rect == playback_rect)
      canvas->discard();

    raster_source->PlaybackToCanvas(
        canvas, resource_lock->color_space_for_raster(), raster_full_rect,
        playback_rect, transform, playback_settings);
  }

  ri->DeleteTextures(1, &texture_id);
}

}  // namespace

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    LayerTreeResourceProvider* resource_provider,
    viz::ResourceId resource_id,
    bool resource_has_previous_content)
    : client_(client),
      lock_(resource_provider, resource_id),
      resource_has_previous_content_(resource_has_previous_content) {
  client_->pending_raster_buffers_.insert(this);
  lock_.CreateMailbox();
}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  client_->pending_raster_buffers_.erase(this);
}

void GpuRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0("cc", "GpuRasterBuffer::Playback");
  client_->PlaybackOnWorkerThread(&lock_, sync_token_,
                                  resource_has_previous_content_, raster_source,
                                  raster_full_rect, raster_dirty_rect,
                                  new_content_id, transform, playback_settings);
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    LayerTreeResourceProvider* resource_provider,
    bool use_distance_field_text,
    int gpu_rasterization_msaa_sample_count,
    viz::ResourceFormat preferred_tile_format,
    bool enable_oop_rasterization)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      msaa_sample_count_(gpu_rasterization_msaa_sample_count),
      preferred_tile_format_(preferred_tile_format),
      enable_oop_rasterization_(enable_oop_rasterization) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() {
  DCHECK(pending_raster_buffers_.empty());
}

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return std::make_unique<RasterBufferImpl>(this, resource_provider_,
                                            resource.gpu_backing_resource_id(),
                                            resource_has_previous_content);
}

void GpuRasterBufferProvider::OrderingBarrier() {
  TRACE_EVENT0("cc", "GpuRasterBufferProvider::OrderingBarrier");

  gpu::gles2::GLES2Interface* gl = compositor_context_provider_->ContextGL();
  gpu::SyncToken sync_token =
      LayerTreeResourceProvider::GenerateSyncTokenHelper(gl);
  for (RasterBufferImpl* buffer : pending_raster_buffers_)
    buffer->set_sync_token(sync_token);
  pending_raster_buffers_.clear();
}

void GpuRasterBufferProvider::Flush() {
  compositor_context_provider_->ContextSupport()->FlushPendingWork();
}

viz::ResourceFormat GpuRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  if (resource_provider_->IsRenderBufferFormatSupported(
          preferred_tile_format_) &&
      (DoesResourceFormatSupportAlpha(preferred_tile_format_) ||
       !must_support_alpha)) {
    return preferred_tile_format_;
  }

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
    const ResourcePool::InUsePoolResource& resource) const {
  gpu::SyncToken sync_token = resource_provider_->GetSyncTokenForResources(
      {resource.gpu_backing_resource_id()});
  if (!sync_token.HasData())
    return true;

  // IsSyncTokenSignaled is thread-safe, no need for worker context lock.
  return worker_context_provider_->ContextSupport()->IsSyncTokenSignaled(
      sync_token);
}

uint64_t GpuRasterBufferProvider::SetReadyToDrawCallback(
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

void GpuRasterBufferProvider::Shutdown() {
  pending_raster_buffers_.clear();
}

void GpuRasterBufferProvider::PlaybackOnWorkerThread(
    LayerTreeResourceProvider::ScopedWriteLockRaster* resource_lock,
    const gpu::SyncToken& sync_token,
    bool resource_has_previous_content,
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings) {
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  DCHECK(ri);

  // Synchronize with compositor. Nop if sync token is empty.
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

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

  if (enable_oop_rasterization_) {
    RasterizeSourceOOP(raster_source, resource_has_previous_content,
                       resource_lock->size(), raster_full_rect, playback_rect,
                       transform, playback_settings, worker_context_provider_,
                       resource_lock, use_distance_field_text_,
                       msaa_sample_count_);
  } else {
    RasterizeSource(raster_source, resource_has_previous_content,
                    resource_lock->size(), raster_full_rect, playback_rect,
                    transform, playback_settings, worker_context_provider_,
                    resource_lock, use_distance_field_text_,
                    msaa_sample_count_);
  }

  // Generate sync token for cross context synchronization.
  resource_lock->set_sync_token(
      LayerTreeResourceProvider::GenerateSyncTokenHelper(ri));
}

}  // namespace cc
