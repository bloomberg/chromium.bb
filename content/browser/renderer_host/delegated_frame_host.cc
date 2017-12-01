// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/time/default_tick_clock.h"
#include "cc/base/switches.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/surfaces/stub_surface_reference_factory.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_hittest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/compositor_resize_lock.h"
#include "content/browser/renderer_host/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost

DelegatedFrameHost::DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                                       DelegatedFrameHostClient* client,
                                       bool enable_surface_synchronization,
                                       bool enable_viz)
    : frame_sink_id_(frame_sink_id),
      client_(client),
      enable_surface_synchronization_(enable_surface_synchronization),
      enable_viz_(enable_viz),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      background_color_(SK_ColorRED),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)),
      weak_ptr_factory_(this) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->AddObserver(this);
  viz::HostFrameSinkManager* host_frame_sink_manager =
      factory->GetContextFactoryPrivate()->GetHostFrameSinkManager();
  host_frame_sink_manager->RegisterFrameSinkId(frame_sink_id_, this);
#if DCHECK_IS_ON()
  host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_,
                                                  "DelegatedFrameHost");
#endif
  CreateCompositorFrameSinkSupport();
}

DelegatedFrameHost::~DelegatedFrameHost() {
  DCHECK(!compositor_);
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->RemoveObserver(this);

  ResetCompositorFrameSinkSupport();

  viz::HostFrameSinkManager* host_frame_sink_manager =
      factory->GetContextFactoryPrivate()->GetHostFrameSinkManager();
  host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);

  DCHECK(!vsync_manager_.get());
}

void DelegatedFrameHost::WasShown(const ui::LatencyInfo& latency_info) {
  frame_evictor_->SetVisible(true);

  if (!has_primary_surface_ && !released_front_lock_.get()) {
    if (compositor_)
      released_front_lock_ = compositor_->GetCompositorLock(nullptr);
  }

  if (compositor_) {
    compositor_->SetLatencyInfo(latency_info);
  }
}

bool DelegatedFrameHost::HasSavedFrame() {
  return frame_evictor_->HasFrame();
}

void DelegatedFrameHost::WasHidden() {
  frame_evictor_->SetVisible(false);
  released_front_lock_ = nullptr;
}

void DelegatedFrameHost::MaybeCreateResizeLock() {
  DCHECK(!resize_lock_);

  if (!compositor_)
    return;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableResizeLock))
    return;

  if (!has_primary_surface_)
    return;

  if (!client_->DelegatedFrameCanCreateResizeLock())
    return;

  gfx::Size desired_size = client_->DelegatedFrameHostDesiredSizeInDIP();
  if (desired_size.IsEmpty())
    return;
  if (desired_size == current_frame_size_in_dip_)
    return;

  resize_lock_ = client_->DelegatedFrameHostCreateResizeLock();
  bool locked = resize_lock_->Lock();
  DCHECK(locked);
}

void DelegatedFrameHost::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  // Only ARGB888 and RGB565 supported as of now.
  bool format_support = ((preferred_color_type == kAlpha_8_SkColorType) ||
                         (preferred_color_type == kRGB_565_SkColorType) ||
                         (preferred_color_type == kN32_SkColorType));
  DCHECK(format_support);
  if (!CanCopyFromCompositingSurface()) {
    callback.Run(SkBitmap(), content::READBACK_SURFACE_UNAVAILABLE);
    return;
  }

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(&CopyFromCompositingSurfaceHasResult, output_size,
                         preferred_color_type, callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);
  RequestCopyOfOutput(std::move(request));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    scoped_refptr<media::VideoFrame> target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  if (!CanCopyFromCompositingSurface()) {
    callback.Run(gfx::Rect(), false);
    return;
  }

  std::unique_ptr<viz::CopyOutputRequest> request = std::make_unique<
      viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      base::BindOnce(
          &DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(),  // For caching the ReadbackYUVInterface on this class.
          nullptr, std::move(target), callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);
  RequestCopyOfOutput(std::move(request));
}

bool DelegatedFrameHost::CanCopyFromCompositingSurface() const {
  return compositor_ &&
         client_->DelegatedFrameHostGetLayer()->has_external_content();
}

void DelegatedFrameHost::BeginFrameSubscription(
    std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscriber_ = std::move(subscriber);
}

void DelegatedFrameHost::EndFrameSubscription() {
  idle_frame_subscriber_textures_.clear();
  frame_subscriber_.reset();
}

viz::FrameSinkId DelegatedFrameHost::GetFrameSinkId() {
  return frame_sink_id_;
}

viz::SurfaceId DelegatedFrameHost::SurfaceIdAtPoint(
    viz::SurfaceHittestDelegate* delegate,
    const gfx::PointF& point,
    gfx::PointF* transformed_point) {
  *transformed_point = point;
  viz::SurfaceId surface_id(frame_sink_id_, local_surface_id_);
  if (!surface_id.is_valid() || enable_viz_)
    return surface_id;
  viz::SurfaceHittest hittest(delegate,
                              GetFrameSinkManager()->surface_manager());
  gfx::Transform target_transform;
  viz::SurfaceId target_local_surface_id = hittest.GetTargetSurfaceAtPoint(
      surface_id, gfx::ToFlooredPoint(point), &target_transform);
  if (target_local_surface_id.is_valid())
    target_transform.TransformPoint(transformed_point);
  return target_local_surface_id;
}

bool DelegatedFrameHost::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  viz::SurfaceId surface_id(frame_sink_id_, local_surface_id_);
  if (!surface_id.is_valid() || enable_viz_)
    return false;
  *transformed_point = point;
  if (original_surface == surface_id)
    return true;

  viz::SurfaceHittest hittest(nullptr,
                              GetFrameSinkManager()->surface_manager());
  return hittest.TransformPointToTargetSurface(original_surface, surface_id,
                                               transformed_point);
}

bool DelegatedFrameHost::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (!has_primary_surface_)
    return false;

  return target_view->TransformPointToLocalCoordSpace(
      point, viz::SurfaceId(frame_sink_id_, local_surface_id_),
      transformed_point);
}

void DelegatedFrameHost::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  needs_begin_frame_ = needs_begin_frames;
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void DelegatedFrameHost::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  DCHECK(!ack.has_damage);
  support_->DidNotProduceFrame(ack);
}

bool DelegatedFrameHost::ShouldSkipFrame(const gfx::Size& size_in_dip) {
  if (!resize_lock_)
    return false;
  // Allow a single renderer frame through even though there's a resize lock
  // currently in place.
  if (allow_one_renderer_frame_during_resize_lock_) {
    allow_one_renderer_frame_during_resize_lock_ = false;
    return false;
  }
  return size_in_dip != resize_lock_->expected_size();
}

void DelegatedFrameHost::OnAggregatedSurfaceDamage(
    const viz::LocalSurfaceId& id,
    const gfx::Rect& damage_rect) {
  if (id != local_surface_id_)
    return;
  AttemptFrameSubscriberCapture(damage_rect);
}

void DelegatedFrameHost::WasResized() {
  if (client_->DelegatedFrameHostDesiredSizeInDIP() !=
          current_frame_size_in_dip_ &&
      !client_->DelegatedFrameHostIsVisible()) {
    EvictDelegatedFrame();
  }

  if (enable_surface_synchronization_) {
    current_frame_size_in_dip_ = client_->DelegatedFrameHostDesiredSizeInDIP();

    viz::SurfaceId surface_id(frame_sink_id_, client_->GetLocalSurfaceId());
    client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
        surface_id, current_frame_size_in_dip_, GetSurfaceReferenceFactory());
    has_primary_surface_ = true;
    frame_evictor_->SwappedFrame(client_->DelegatedFrameHostIsVisible());
    if (compositor_)
      compositor_->OnChildResizing();
    // Input throttling and guttering are handled differently when surface
    // synchronization is enabled so exit early here.
    return;
  }

  // If |create_resize_lock_after_commit_| is true, we're waiting to recreate
  // an expired resize lock after the next UI frame is submitted, so don't
  // make a lock here.
  if (!resize_lock_ && !create_resize_lock_after_commit_)
    MaybeCreateResizeLock();
  UpdateGutters();
}

SkColor DelegatedFrameHost::GetGutterColor() const {
  // In fullscreen mode resizing is uncommon, so it makes more sense to
  // make the initial switch to fullscreen mode look better by using black as
  // the gutter color.
  return client_->DelegatedFrameHostGetGutterColor(background_color_);
}

void DelegatedFrameHost::UpdateGutters() {
  if (!has_primary_surface_ || enable_surface_synchronization_) {
    right_gutter_.reset();
    bottom_gutter_.reset();
    return;
  }

  if (current_frame_size_in_dip_.width() <
      client_->DelegatedFrameHostDesiredSizeInDIP().width()) {
    right_gutter_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    right_gutter_->SetColor(GetGutterColor());
    int width = client_->DelegatedFrameHostDesiredSizeInDIP().width() -
                current_frame_size_in_dip_.width();
    // The right gutter also includes the bottom-right corner, if necessary.
    int height = client_->DelegatedFrameHostDesiredSizeInDIP().height();
    right_gutter_->SetBounds(
        gfx::Rect(current_frame_size_in_dip_.width(), 0, width, height));

    client_->DelegatedFrameHostGetLayer()->Add(right_gutter_.get());
  } else {
    right_gutter_.reset();
  }

  if (current_frame_size_in_dip_.height() <
      client_->DelegatedFrameHostDesiredSizeInDIP().height()) {
    bottom_gutter_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    bottom_gutter_->SetColor(GetGutterColor());
    int width = current_frame_size_in_dip_.width();
    int height = client_->DelegatedFrameHostDesiredSizeInDIP().height() -
                 current_frame_size_in_dip_.height();
    bottom_gutter_->SetBounds(
        gfx::Rect(0, current_frame_size_in_dip_.height(), width, height));
    client_->DelegatedFrameHostGetLayer()->Add(bottom_gutter_.get());
  } else {
    bottom_gutter_.reset();
  }
}

gfx::Size DelegatedFrameHost::GetRequestedRendererSize() const {
  if (resize_lock_)
    return resize_lock_->expected_size();
  else
    return client_->DelegatedFrameHostDesiredSizeInDIP();
}

void DelegatedFrameHost::CheckResizeLock() {
  if (!resize_lock_ ||
      resize_lock_->expected_size() != current_frame_size_in_dip_)
    return;

  // Since we got the size we were looking for, unlock the compositor. But delay
  // the release of the lock until we've kicked a frame with the new texture, to
  // avoid resizing the UI before we have a chance to draw a "good" frame.
  resize_lock_->UnlockCompositor();
}

void DelegatedFrameHost::AttemptFrameSubscriberCapture(
    const gfx::Rect& damage_rect) {
  if (!frame_subscriber() || !CanCopyFromCompositingSurface())
    return;

  const base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeTicks present_time;
  if (vsync_interval_ <= base::TimeDelta()) {
    present_time = now;
  } else {
    const int64_t intervals_elapsed = (now - vsync_timebase_) / vsync_interval_;
    present_time = vsync_timebase_ + (intervals_elapsed + 1) * vsync_interval_;
  }

  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
  if (!frame_subscriber()->ShouldCaptureFrame(damage_rect, present_time, &frame,
                                              &callback))
    return;

  // Get a texture to re-use; else, create a new one.
  scoped_refptr<OwnedMailbox> subscriber_texture;
  if (!idle_frame_subscriber_textures_.empty()) {
    subscriber_texture = idle_frame_subscriber_textures_.back();
    idle_frame_subscriber_textures_.pop_back();
  } else if (viz::GLHelper* helper =
                 ImageTransportFactory::GetInstance()->GetGLHelper()) {
    subscriber_texture = new OwnedMailbox(helper);
  }

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(
              &DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo,
              AsWeakPtr(), subscriber_texture, frame,
              base::Bind(callback, present_time)));
  // Setting the source in this copy request asks that the layer abort any prior
  // uncommitted copy requests made on behalf of the same frame subscriber.
  // This will not affect any of the copy requests spawned elsewhere from
  // DelegatedFrameHost (e.g., a call to CopyFromCompositingSurface() for
  // screenshots) since those copy requests do not specify |frame_subscriber()|
  // as a source.
  request->set_source(frame_subscriber()->GetSourceIdForCopyRequest());
  if (subscriber_texture) {
    request->SetMailbox(subscriber_texture->mailbox(),
                        subscriber_texture->sync_token());
  }

  // To avoid unnecessary browser composites, try to go directly to the Surface
  // rather than through the Layer (which goes through the browser compositor).
  if (has_primary_surface_ &&
      request_copy_of_output_callback_for_testing_.is_null()) {
    support_->RequestCopyOfSurface(std::move(request));
  } else {
    RequestCopyOfOutput(std::move(request));
  }
}

void DelegatedFrameHost::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  ResetCompositorFrameSinkSupport();
  renderer_compositor_frame_sink_ = renderer_compositor_frame_sink;
  CreateCompositorFrameSinkSupport();
}

void DelegatedFrameHost::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    viz::mojom::HitTestRegionListPtr hit_test_region_list) {
#if defined(OS_CHROMEOS)
  DCHECK(!resize_lock_ || !client_->IsAutoResizeEnabled());
#endif
  float frame_device_scale_factor = frame.metadata.device_scale_factor;
  viz::BeginFrameAck ack(frame.metadata.begin_frame_ack);

  DCHECK(!frame.render_pass_list.empty());

  viz::RenderPass* root_pass = frame.render_pass_list.back().get();

  gfx::Size frame_size = root_pass->output_rect.size();
  gfx::Size frame_size_in_dip =
      gfx::ConvertSizeToDIP(frame_device_scale_factor, frame_size);

  if (ShouldSkipFrame(frame_size_in_dip)) {
    std::vector<viz::ReturnedResource> resources =
        viz::TransferableResource::ReturnResources(frame.resource_list);

    skipped_latency_info_list_.insert(skipped_latency_info_list_.end(),
                                      frame.metadata.latency_info.begin(),
                                      frame.metadata.latency_info.end());

    renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);

    skipped_frames_ = true;
    ack.has_damage = false;
    DidNotProduceFrame(ack);
    return;
  }

  // If we are allowing one renderer frame through, this would ensure the frame
  // gets through even if we regrab the lock after the UI compositor makes one
  // frame. If the renderer frame beats the UI compositor, then we don't need to
  // allow any more, though.
  allow_one_renderer_frame_during_resize_lock_ = false;

  if (skipped_frames_) {
    skipped_frames_ = false;

    // Give the same damage rect to the compositor.
    viz::RenderPass* root_pass = frame.render_pass_list.back().get();
    root_pass->damage_rect = gfx::Rect(frame_size);
  }

  background_color_ = frame.metadata.root_background_color;

  if (frame_size.IsEmpty()) {
    DCHECK(frame.resource_list.empty());
    EvictDelegatedFrame();
  } else {
    frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                       skipped_latency_info_list_.begin(),
                                       skipped_latency_info_list_.end());
    skipped_latency_info_list_.clear();

    // If surface synchronization is off, then OnFirstSurfaceActivation will be
    // called in the same call stack.
    bool result = support_->SubmitCompositorFrame(
        local_surface_id, std::move(frame), std::move(hit_test_region_list));
    DCHECK(result);

    DCHECK(enable_surface_synchronization_ || has_primary_surface_);
  }

  if (!enable_surface_synchronization_) {
    if (has_primary_surface_)
      frame_evictor_->SwappedFrame(client_->DelegatedFrameHostIsVisible());
    // Note: the frame may have been evicted immediately.
  }
}

void DelegatedFrameHost::ClearDelegatedFrame() {
  // Ensure that we are able to swap in a new blank frame to replace any old
  // content. This will result in a white flash if we switch back to this
  // content.
  // https://crbug.com/739621
  released_front_lock_.reset();
  EvictDelegatedFrame();
}

void DelegatedFrameHost::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);
}

void DelegatedFrameHost::DidPresentCompositorFrame(uint32_t presentation_token,
                                                   base::TimeTicks time,
                                                   base::TimeDelta refresh,
                                                   uint32_t flags) {
  renderer_compositor_frame_sink_->DidPresentCompositorFrame(
      presentation_token, time, refresh, flags);
}

void DelegatedFrameHost::DidDiscardCompositorFrame(
    uint32_t presentation_token) {
  renderer_compositor_frame_sink_->DidDiscardCompositorFrame(
      presentation_token);
}

void DelegatedFrameHost::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->ReclaimResources(resources);
}

void DelegatedFrameHost::OnBeginFramePausedChanged(bool paused) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFramePausedChanged(paused);
}

void DelegatedFrameHost::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  gfx::Size frame_size_in_dip = gfx::ConvertSizeToDIP(
      surface_info.device_scale_factor(), surface_info.size_in_pixels());

  if (!enable_surface_synchronization_) {
    client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
        surface_info.id(), frame_size_in_dip, GetSurfaceReferenceFactory());
    has_primary_surface_ = true;
  }

  // If surface synchronization is enabled, and we don't have a primary surface
  // then that means it has been evicted, and so we have nothing to do here.
  if (!has_primary_surface_)
    return;

  client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
      surface_info.id());
  local_surface_id_ = surface_info.id().local_surface_id();

  // Surface synchronization deals with resizes in WasResized().
  if (enable_surface_synchronization_)
    return;

  released_front_lock_ = nullptr;
  current_frame_size_in_dip_ = frame_size_in_dip;
  CheckResizeLock();

  UpdateGutters();
}

void DelegatedFrameHost::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

void DelegatedFrameHost::OnBeginFrame(const viz::BeginFrameArgs& args) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFrame(args);
  client_->OnBeginFrame();
}

void DelegatedFrameHost::EvictDelegatedFrame() {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  if (!has_primary_surface_)
    return;
  client_->DelegatedFrameHostGetLayer()->SetShowSolidColorContent();
  support_->EvictCurrentSurface();
  has_primary_surface_ = false;
  resize_lock_.reset();
  frame_evictor_->DiscardedFrame();
  UpdateGutters();
}

// static
void DelegatedFrameHost::ReturnSubscriberTexture(
    base::WeakPtr<DelegatedFrameHost> dfh,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    const gpu::SyncToken& sync_token) {
  if (!subscriber_texture.get())
    return;
  if (!dfh)
    return;

  subscriber_texture->UpdateSyncToken(sync_token);

  if (dfh->frame_subscriber_ && subscriber_texture->texture_id())
    dfh->idle_frame_subscriber_textures_.push_back(subscriber_texture);
}

// static
void DelegatedFrameHost::CopyFromCompositingSurfaceFinishedForVideo(
    scoped_refptr<media::VideoFrame> video_frame,
    base::WeakPtr<DelegatedFrameHost> dfh,
    const base::Callback<void(bool)>& callback,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    bool result) {
  callback.Run(result);

  gpu::SyncToken sync_token;
  if (result) {
    viz::GLHelper* gl_helper =
        ImageTransportFactory::GetInstance()->GetGLHelper();
    gl_helper->GenerateSyncToken(&sync_token);
  }
  if (release_callback) {
    const bool lost_resource = !sync_token.HasData();
    release_callback->Run(sync_token, lost_resource);
  }
  ReturnSubscriberTexture(dfh, subscriber_texture, sync_token);
}

// static
void DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo(
    base::WeakPtr<DelegatedFrameHost> dfh,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    scoped_refptr<media::VideoFrame> video_frame,
    const base::Callback<void(const gfx::Rect&, bool)>& callback,
    std::unique_ptr<viz::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::BindOnce(callback, gfx::Rect(), false));
  base::ScopedClosureRunner scoped_return_subscriber_texture(base::BindOnce(
      &ReturnSubscriberTexture, dfh, subscriber_texture, gpu::SyncToken()));

  if (!dfh)
    return;
  if (result->IsEmpty())
    return;

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's visible_rect() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
      video_frame->visible_rect(), result->size());
  region_in_frame =
      gfx::Rect(region_in_frame.x() & ~1, region_in_frame.y() & ~1,
                region_in_frame.width() & ~1, region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return;

  if (result->format() == viz::CopyOutputResult::Format::RGBA_BITMAP) {
    SkBitmap bitmap = result->AsSkBitmap();
    // Scale the bitmap to the required size, if necessary.
    SkBitmap scaled_bitmap;
    if (result->size() != region_in_frame.size()) {
      skia::ImageOperations::ResizeMethod method =
          skia::ImageOperations::RESIZE_GOOD;
      scaled_bitmap = skia::ImageOperations::Resize(
          bitmap, method, region_in_frame.width(), region_in_frame.height());
    } else {
      scaled_bitmap = bitmap;
    }

    media::CopyRGBToVideoFrame(
        reinterpret_cast<uint8_t*>(scaled_bitmap.getPixels()),
        scaled_bitmap.rowBytes(), region_in_frame, video_frame.get());

    ignore_result(scoped_callback_runner.Release());
    callback.Run(region_in_frame, true);
    return;
  }

  DCHECK_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_TEXTURE);

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  viz::GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;
  if (subscriber_texture.get() && !subscriber_texture->texture_id())
    return;

  gpu::Mailbox mailbox = result->GetTextureResult()->mailbox;
  gpu::SyncToken sync_token = result->GetTextureResult()->sync_token;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback =
      result->TakeTextureOwnership();

  if (!dfh->yuv_readback_pipeline_) {
    dfh->yuv_readback_pipeline_ =
        gl_helper->CreateReadbackPipelineYUV(true, true);
  }
  viz::ReadbackYUVInterface* yuv_readback_pipeline =
      dfh->yuv_readback_pipeline_.get();
  viz::GLHelper::ScalerInterface* const scaler =
      yuv_readback_pipeline->scaler();
  const gfx::Vector2d scale_from(result->size().width(),
                                 result->size().height());
  const gfx::Vector2d scale_to(region_in_frame.width(),
                               region_in_frame.height());
  if (scale_from == scale_to) {
    if (scaler)
      yuv_readback_pipeline->SetScaler(nullptr);
  } else if (!scaler || !scaler->IsSameScaleRatio(scale_from, scale_to)) {
    // The scaler chosen here is based on performance measurements of full
    // end-to-end systems.  When down-scaling, always use the "fast" scaler
    // because it performs well on both low- and high- end machines, provides
    // decent image quality, and doesn't overwhelm downstream video encoders
    // with too much entropy (which can drastically increase CPU utilization).
    // When up-scaling, always use "best" because the quality improvement is
    // huge with insignificant performance penalty.  Note that this strategy
    // differs from single-frame snapshot capture.
    viz::GLHelper::ScalerQuality quality =
        ((result->size().width() < region_in_frame.size().width()) &&
         (result->size().height() < region_in_frame.size().height()))
            ? viz::GLHelper::SCALER_QUALITY_BEST
            : viz::GLHelper::SCALER_QUALITY_FAST;

    DVLOG(1) << "Re-creating YUV readback pipeline scaler for source rect "
             << result->rect().ToString() << " and destination size "
             << region_in_frame.size().ToString();

    std::unique_ptr<viz::GLHelper::ScalerInterface> scaler =
        gl_helper->CreateScaler(quality, scale_from, scale_to, false, false,
                                false);
    DCHECK(scaler);  // Arguments to CreateScaler() should never be invalid.
    yuv_readback_pipeline->SetScaler(std::move(scaler));
  }

  ignore_result(scoped_callback_runner.Release());
  ignore_result(scoped_return_subscriber_texture.Release());

  base::Callback<void(bool result)> finished_callback = base::Bind(
      &DelegatedFrameHost::CopyFromCompositingSurfaceFinishedForVideo,
      video_frame, dfh->AsWeakPtr(), base::Bind(callback, region_in_frame),
      subscriber_texture, base::Passed(&release_callback));
  yuv_readback_pipeline->ReadbackYUV(
      mailbox, sync_token, result->size(), gfx::Rect(region_in_frame.size()),
      video_frame->stride(media::VideoFrame::kYPlane),
      video_frame->data(media::VideoFrame::kYPlane),
      video_frame->stride(media::VideoFrame::kUPlane),
      video_frame->data(media::VideoFrame::kUPlane),
      video_frame->stride(media::VideoFrame::kVPlane),
      video_frame->data(media::VideoFrame::kVPlane), region_in_frame.origin(),
      finished_callback);
  media::LetterboxYUV(video_frame.get(), region_in_frame);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::CompositorObserver implementation:

void DelegatedFrameHost::OnCompositingDidCommit(ui::Compositor* compositor) {
  // If |create_resize_lock_after_commit_| then we should have popped the old
  // lock already.
  DCHECK(!resize_lock_ || !create_resize_lock_after_commit_);

  if (resize_lock_ &&
      resize_lock_->expected_size() == current_frame_size_in_dip_) {
    resize_lock_.reset();
    // We had a lock but the UI may have resized in the meantime.
    create_resize_lock_after_commit_ = true;
  }

  if (create_resize_lock_after_commit_) {
    create_resize_lock_after_commit_ = false;
    MaybeCreateResizeLock();
  }
}

void DelegatedFrameHost::OnCompositingStarted(ui::Compositor* compositor,
                                              base::TimeTicks start_time) {
  last_draw_ended_ = start_time;
}

void DelegatedFrameHost::OnCompositingEnded(ui::Compositor* compositor) {}

void DelegatedFrameHost::OnCompositingLockStateChanged(
    ui::Compositor* compositor) {
  if (resize_lock_ && resize_lock_->timed_out()) {
    // A compositor lock that is part of a resize lock timed out. We allow
    // the UI to produce a frame before locking it again, so we don't lock here.
    // We release the |resize_lock_| though to allow any other resizes that are
    // desired at the same time since we're allowing the UI to make a frame
    // which will gutter anyways.
    resize_lock_.reset();
    create_resize_lock_after_commit_ = true;
    // Because this timed out, we're going to allow the UI to update and lock
    // again. We would allow renderer frames through during this time if they
    // came late, but would stop them again once the UI finished its frame. We
    // want to allow the slow renderer to show us one frame even if its wrong
    // since we're guttering anyways, but not unlimited number of frames as that
    // would be a waste of power.
    allow_one_renderer_frame_during_resize_lock_ = true;
  }
}

void DelegatedFrameHost::OnCompositingChildResizing(
    ui::Compositor* compositor) {}

void DelegatedFrameHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor, compositor_);
  ResetCompositor();
  DCHECK(!compositor_);
}

void DelegatedFrameHost::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ImageTransportFactoryObserver implementation:

void DelegatedFrameHost::OnLostResources() {
  EvictDelegatedFrame();
  idle_frame_subscriber_textures_.clear();
  yuv_readback_pipeline_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

void DelegatedFrameHost::SetCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
  if (!compositor)
    return;
  compositor_ = compositor;
  compositor_->AddObserver(this);
  DCHECK(!vsync_manager_.get());
  vsync_manager_ = compositor_->vsync_manager();
  vsync_manager_->AddObserver(this);

  compositor_->AddFrameSink(frame_sink_id_);
}

void DelegatedFrameHost::ResetCompositor() {
  if (!compositor_)
    return;
  resize_lock_.reset();
  if (compositor_->HasObserver(this))
    compositor_->RemoveObserver(this);
  if (vsync_manager_) {
    vsync_manager_->RemoveObserver(this);
    vsync_manager_ = nullptr;
  }

  compositor_->RemoveFrameSink(frame_sink_id_);
  compositor_ = nullptr;
}

void DelegatedFrameHost::LockResources() {
  DCHECK(local_surface_id_.is_valid());
  frame_evictor_->LockFrame();
}

void DelegatedFrameHost::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  // If a specific area has not been requested, set one to ensure correct
  // clipping occurs.
  if (!request->has_area())
    request->set_area(gfx::Rect(current_frame_size_in_dip_));

  if (request_copy_of_output_callback_for_testing_.is_null()) {
    client_->DelegatedFrameHostGetLayer()->RequestCopyOfOutput(
        std::move(request));
  } else {
    request_copy_of_output_callback_for_testing_.Run(std::move(request));
  }
}

void DelegatedFrameHost::UnlockResources() {
  DCHECK(local_surface_id_.is_valid());
  frame_evictor_->UnlockFrame();
}

void DelegatedFrameHost::CreateCompositorFrameSinkSupport() {
  if (enable_viz_)
    return;

  DCHECK(!support_);
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  support_ = factory->GetContextFactoryPrivate()
                 ->GetHostFrameSinkManager()
                 ->CreateCompositorFrameSinkSupport(this, frame_sink_id_,
                                                    is_root, needs_sync_points);
  support_->SetAggregatedDamageCallback(
      base::BindRepeating(&DelegatedFrameHost::OnAggregatedSurfaceDamage,
                          weak_ptr_factory_.GetWeakPtr()));
  if (compositor_)
    compositor_->AddFrameSink(frame_sink_id_);
  if (needs_begin_frame_)
    support_->SetNeedsBeginFrame(true);
}

void DelegatedFrameHost::ResetCompositorFrameSinkSupport() {
  if (!support_)
    return;
  if (compositor_)
    compositor_->RemoveFrameSink(frame_sink_id_);
  support_.reset();
}

scoped_refptr<viz::SurfaceReferenceFactory>
DelegatedFrameHost::GetSurfaceReferenceFactory() {
  if (enable_viz_)
    return base::MakeRefCounted<viz::StubSurfaceReferenceFactory>();

  return GetFrameSinkManager()->surface_manager()->reference_factory();
}

}  // namespace content
