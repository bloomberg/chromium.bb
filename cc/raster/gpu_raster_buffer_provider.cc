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
#include "components/viz/common/gpu/texture_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gl/trace_util.h"

namespace cc {
namespace {

static void RasterizeSourceOOP(
    const RasterSource* raster_source,
    bool resource_has_previous_content,
    const gpu::Mailbox& mailbox,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    bool texture_storage_allocated,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    viz::RasterContextProvider* context_provider,
    bool use_distance_field_text,
    int msaa_sample_count) {
  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  GLuint texture_id = ri->CreateAndConsumeTextureCHROMIUM(mailbox.name);
  if (!texture_storage_allocated) {
    viz::TextureAllocation alloc = {texture_id, texture_target,
                                    texture_is_overlay_candidate};
    viz::TextureAllocation::AllocateStorage(
        ri, context_provider->ContextCapabilities(), resource_format,
        resource_size, alloc, color_space);
  }

  // TODO(enne): Use the |texture_target|? GpuMemoryBuffer backed textures don't
  // use GL_TEXTURE_2D.
  ri->BeginRasterCHROMIUM(
      texture_id, raster_source->background_color(), msaa_sample_count,
      playback_settings.use_lcd_text, use_distance_field_text,
      viz::ResourceFormatToClosestSkColorType(resource_format),
      playback_settings.raster_color_space);
  float recording_to_raster_scale =
      transform.scale() / raster_source->recording_scale_factor();
  gfx::Size content_size = raster_source->GetContentSize(transform.scale());
  // TODO(enne): could skip the clear on new textures, as the service side has
  // to do that anyway.  resource_has_previous_content implies that the texture
  // is not new, but the reverse does not hold, so more plumbing is needed.
  ri->RasterCHROMIUM(raster_source->GetDisplayItemList().get(),
                     playback_settings.image_provider, content_size,
                     raster_full_rect, playback_rect, transform.translation(),
                     recording_to_raster_scale,
                     raster_source->requires_clear());
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
    const gpu::Mailbox& mailbox,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    bool texture_storage_allocated,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& playback_rect,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings,
    viz::RasterContextProvider* context_provider,
    bool use_distance_field_text,
    int msaa_sample_count) {
  ScopedGrContextAccess gr_context_access(context_provider);

  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  GLuint texture_id = ri->CreateAndConsumeTextureCHROMIUM(mailbox.name);
  if (!texture_storage_allocated) {
    viz::TextureAllocation alloc = {texture_id, texture_target,
                                    texture_is_overlay_candidate};
    viz::TextureAllocation::AllocateStorage(
        ri, context_provider->ContextCapabilities(), resource_format,
        resource_size, alloc, color_space);
  }

  {
    LayerTreeResourceProvider::ScopedSkSurface scoped_surface(
        context_provider->GrContext(), texture_id, texture_target,
        resource_size, resource_format, use_distance_field_text,
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

    gfx::Size content_size = raster_source->GetContentSize(transform.scale());
    raster_source->PlaybackToCanvas(canvas, color_space, content_size,
                                    raster_full_rect, playback_rect, transform,
                                    playback_settings);
  }

  ri->DeleteTextures(1, &texture_id);
}

}  // namespace

// Subclass for InUsePoolResource that holds ownership of a gpu-rastered backing
// and does cleanup of the backing when destroyed.
class GpuRasterBufferProvider::GpuRasterBacking
    : public ResourcePool::GpuBacking {
 public:
  ~GpuRasterBacking() override {
    gpu::gles2::GLES2Interface* gl = compositor_context_provider->ContextGL();
    if (returned_sync_token.HasData())
      gl->WaitSyncTokenCHROMIUM(returned_sync_token.GetConstData());
    if (mailbox_sync_token.HasData())
      gl->WaitSyncTokenCHROMIUM(mailbox_sync_token.GetConstData());
    if (texture_id)
      gl->DeleteTextures(1, &texture_id);
  }

  base::trace_event::MemoryAllocatorDumpGuid MemoryDumpGuid(
      uint64_t tracing_process_id) override {
    if (!storage_allocated)
      return {};
    return gl::GetGLTextureClientGUIDForTracing(
        compositor_context_provider->ContextSupport()->ShareGroupTracingGUID(),
        texture_id);
  }
  base::UnguessableToken SharedMemoryGuid() override { return {}; }

  // The ContextProvider used to clean up the texture id.
  viz::ContextProvider* compositor_context_provider = nullptr;
  // The texture backing of the resource.
  GLuint texture_id = 0;
  // The allocation of storage for the |texture_id| is deferred, and this tracks
  // if it has been done.
  bool storage_allocated = false;
};

GpuRasterBufferProvider::RasterBufferImpl::RasterBufferImpl(
    GpuRasterBufferProvider* client,
    const ResourcePool::InUsePoolResource& in_use_resource,
    GpuRasterBacking* backing,
    const gpu::SyncToken& before_raster_sync_token,
    bool resource_has_previous_content)
    : client_(client),
      backing_(backing),
      resource_size_(in_use_resource.size()),
      resource_format_(in_use_resource.format()),
      color_space_(in_use_resource.color_space()),
      resource_has_previous_content_(resource_has_previous_content),
      before_raster_sync_token_(before_raster_sync_token),
      mailbox_(backing->mailbox),
      texture_target_(backing->texture_target),
      texture_is_overlay_candidate_(backing->overlay_candidate),
      texture_storage_allocated_(backing->storage_allocated) {}

GpuRasterBufferProvider::RasterBufferImpl::~RasterBufferImpl() {
  // This SyncToken was created on the worker context after rastering the
  // texture content.
  backing_->mailbox_sync_token = after_raster_sync_token_;
  if (after_raster_sync_token_.HasData()) {
    // The returned SyncToken was waited on in Playback. We know Playback
    // happened if the |after_raster_sync_token_| was set.
    backing_->returned_sync_token = gpu::SyncToken();
  }
  backing_->storage_allocated = texture_storage_allocated_;
}

void GpuRasterBufferProvider::RasterBufferImpl::Playback(
    const RasterSource* raster_source,
    const gfx::Rect& raster_full_rect,
    const gfx::Rect& raster_dirty_rect,
    uint64_t new_content_id,
    const gfx::AxisTransform2d& transform,
    const RasterSource::PlaybackSettings& playback_settings) {
  TRACE_EVENT0("cc", "GpuRasterBuffer::Playback");
  // The |before_raster_sync_token_| passed in here was created on the
  // compositor thread, or given back with the texture for reuse. This call
  // returns another SyncToken generated on the worker thread to synchronize
  // with after the raster is complete.
  after_raster_sync_token_ = client_->PlaybackOnWorkerThread(
      mailbox_, texture_target_, texture_is_overlay_candidate_,
      texture_storage_allocated_, before_raster_sync_token_, resource_size_,
      resource_format_, color_space_, resource_has_previous_content_,
      raster_source, raster_full_rect, raster_dirty_rect, new_content_id,
      transform, playback_settings);
  texture_storage_allocated_ = true;
}

GpuRasterBufferProvider::GpuRasterBufferProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::RasterContextProvider* worker_context_provider,
    LayerTreeResourceProvider* resource_provider,
    bool use_distance_field_text,
    bool use_gpu_memory_buffer_resources,
    int gpu_rasterization_msaa_sample_count,
    viz::ResourceFormat preferred_tile_format,
    bool enable_oop_rasterization)
    : compositor_context_provider_(compositor_context_provider),
      worker_context_provider_(worker_context_provider),
      resource_provider_(resource_provider),
      use_distance_field_text_(use_distance_field_text),
      use_gpu_memory_buffer_resources_(use_gpu_memory_buffer_resources),
      msaa_sample_count_(gpu_rasterization_msaa_sample_count),
      preferred_tile_format_(preferred_tile_format),
      enable_oop_rasterization_(enable_oop_rasterization) {
  DCHECK(compositor_context_provider);
  DCHECK(worker_context_provider);
}

GpuRasterBufferProvider::~GpuRasterBufferProvider() {
}

std::unique_ptr<RasterBuffer> GpuRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  gpu::SyncToken before_raster_sync_token;
  bool new_resource = false;
  if (!resource.gpu_backing()) {
    auto backing = std::make_unique<GpuRasterBacking>();
    backing->compositor_context_provider = compositor_context_provider_;

    gpu::gles2::GLES2Interface* gl = compositor_context_provider_->ContextGL();
    const auto& caps = compositor_context_provider_->ContextCapabilities();

    viz::TextureAllocation alloc = viz::TextureAllocation::MakeTextureId(
        gl, caps, resource.format(), use_gpu_memory_buffer_resources_,
        /*for_framebuffer_attachment=*/true);
    backing->texture_id = alloc.texture_id;
    backing->texture_target = alloc.texture_target;
    backing->overlay_candidate = alloc.overlay_candidate;
    backing->mailbox = gpu::Mailbox::Generate();
    gl->ProduceTextureDirectCHROMIUM(backing->texture_id,
                                     backing->mailbox.name);
    before_raster_sync_token =
        LayerTreeResourceProvider::GenerateSyncTokenHelper(gl);

    resource.set_gpu_backing(std::move(backing));
    new_resource = true;
  }
  GpuRasterBacking* backing =
      static_cast<GpuRasterBacking*>(resource.gpu_backing());
  if (!new_resource)
    before_raster_sync_token = backing->returned_sync_token;

  bool resource_has_previous_content =
      resource_content_id && resource_content_id == previous_content_id;
  return std::make_unique<RasterBufferImpl>(this, resource, backing,
                                            before_raster_sync_token,
                                            resource_has_previous_content);
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
  const gpu::SyncToken& sync_token = resource.gpu_backing()->mailbox_sync_token;
  // This SyncToken() should have been set by calling OrderingBarrier() before
  // calling this.
  DCHECK(sync_token.HasData());

  // IsSyncTokenSignaled is thread-safe, no need for worker context lock.
  return worker_context_provider_->ContextSupport()->IsSyncTokenSignaled(
      sync_token);
}

uint64_t GpuRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  gpu::SyncToken latest_sync_token;
  for (const auto* in_use : resources) {
    const gpu::SyncToken& sync_token =
        in_use->gpu_backing()->mailbox_sync_token;
    if (sync_token.release_count() > latest_sync_token.release_count())
      latest_sync_token = sync_token;
  }
  uint64_t callback_id = latest_sync_token.release_count();
  DCHECK_NE(callback_id, 0u);

  // If the callback is different from the one the caller is already waiting on,
  // pass the callback through to SignalSyncToken. Otherwise the request is
  // redundant.
  if (callback_id != pending_callback_id) {
    // Use the compositor context because we want this callback on the
    // compositor thread.
    compositor_context_provider_->ContextSupport()->SignalSyncToken(
        latest_sync_token, callback);
  }

  return callback_id;
}

void GpuRasterBufferProvider::Shutdown() {
}

gpu::SyncToken GpuRasterBufferProvider::PlaybackOnWorkerThread(
    const gpu::Mailbox& mailbox,
    GLenum texture_target,
    bool texture_is_overlay_candidate,
    bool texture_storage_allocated,
    const gpu::SyncToken& sync_token,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
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

  // Wait on the SyncToken that was created on the compositor thread after
  // making the mailbox. This ensures that the mailbox we consume here is valid
  // by the time the consume command executes.
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
    RasterizeSourceOOP(
        raster_source, resource_has_previous_content, mailbox, texture_target,
        texture_is_overlay_candidate, texture_storage_allocated, resource_size,
        resource_format, color_space, raster_full_rect, playback_rect,
        transform, playback_settings, worker_context_provider_,
        use_distance_field_text_, msaa_sample_count_);
  } else {
    RasterizeSource(raster_source, resource_has_previous_content, mailbox,
                    texture_target, texture_is_overlay_candidate,
                    texture_storage_allocated, resource_size, resource_format,
                    color_space, raster_full_rect, playback_rect, transform,
                    playback_settings, worker_context_provider_,
                    use_distance_field_text_, msaa_sample_count_);
  }

  // Generate sync token for cross context synchronization.
  return LayerTreeResourceProvider::GenerateSyncTokenHelper(ri);
}

}  // namespace cc
