// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/delegated_frame_host.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/time/default_tick_clock.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_hittest.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/resize_lock.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

namespace {

void SatisfyCallback(cc::SurfaceManager* manager,
                     cc::SurfaceSequence sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

void RequireCallback(cc::SurfaceManager* manager,
                     cc::SurfaceId id,
                     cc::SurfaceSequence sequence) {
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost

DelegatedFrameHost::DelegatedFrameHost(DelegatedFrameHostClient* client)
    : client_(client),
      compositor_(nullptr),
      use_surfaces_(true),
      tick_clock_(new base::DefaultTickClock()),
      last_output_surface_id_(0),
      pending_delegated_ack_count_(0),
      skipped_frames_(false),
      current_scale_factor_(1.f),
      can_lock_compositor_(YES_CAN_LOCK),
      delegated_frame_evictor_(new DelegatedFrameEvictor(this)) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->AddObserver(this);
  id_allocator_ = factory->GetContextFactory()->CreateSurfaceIdAllocator();
}

void DelegatedFrameHost::WasShown(const ui::LatencyInfo& latency_info) {
  delegated_frame_evictor_->SetVisible(true);

  if (surface_id_.is_null() && !frame_provider_.get() &&
      !released_front_lock_.get()) {
    if (compositor_)
      released_front_lock_ = compositor_->GetCompositorLock();
  }

  if (compositor_) {
    compositor_->SetLatencyInfo(latency_info);
  }
}

bool DelegatedFrameHost::HasSavedFrame() {
  return delegated_frame_evictor_->HasFrame();
}

void DelegatedFrameHost::WasHidden() {
  delegated_frame_evictor_->SetVisible(false);
  released_front_lock_ = NULL;
}

void DelegatedFrameHost::MaybeCreateResizeLock() {
  if (!ShouldCreateResizeLock())
    return;
  DCHECK(compositor_);

  bool defer_compositor_lock =
      can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT;

  if (can_lock_compositor_ == YES_CAN_LOCK)
    can_lock_compositor_ = YES_DID_LOCK;

  resize_lock_ =
      client_->DelegatedFrameHostCreateResizeLock(defer_compositor_lock);
}

bool DelegatedFrameHost::ShouldCreateResizeLock() {
  if (!client_->DelegatedFrameCanCreateResizeLock())
    return false;

  if (resize_lock_)
    return false;

  gfx::Size desired_size = client_->DelegatedFrameHostDesiredSizeInDIP();
  if (desired_size == current_frame_size_in_dip_ || desired_size.IsEmpty())
    return false;

  if (!compositor_)
    return false;

  return true;
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
  if (!CanCopyToBitmap()) {
    callback.Run(SkBitmap(), content::READBACK_SURFACE_UNAVAILABLE);
    return;
  }

  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(
          base::Bind(&DelegatedFrameHost::CopyFromCompositingSurfaceHasResult,
                     output_size, preferred_color_type, callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);
  RequestCopyOfOutput(std::move(request));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  if (!CanCopyToVideoFrame()) {
    callback.Run(gfx::Rect(), false);
    return;
  }

  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &DelegatedFrameHost::
               CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(),  // For caching the ReadbackYUVInterface on this class.
          nullptr,
          target,
          callback));
  request->set_area(src_subrect);
  RequestCopyOfOutput(std::move(request));
}

bool DelegatedFrameHost::CanCopyToBitmap() const {
  return compositor_ &&
         client_->DelegatedFrameHostGetLayer()->has_external_content();
}

bool DelegatedFrameHost::CanCopyToVideoFrame() const {
  return compositor_ &&
         client_->DelegatedFrameHostGetLayer()->has_external_content();
}

void DelegatedFrameHost::BeginFrameSubscription(
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscriber_ = std::move(subscriber);
}

void DelegatedFrameHost::EndFrameSubscription() {
  idle_frame_subscriber_textures_.clear();
  frame_subscriber_.reset();
}

uint32_t DelegatedFrameHost::GetSurfaceIdNamespace() {
  if (!use_surfaces_)
    return 0;

  return id_allocator_->id_namespace();
}

cc::SurfaceId DelegatedFrameHost::SurfaceIdAtPoint(
    cc::SurfaceHittestDelegate* delegate,
    const gfx::Point& point,
    gfx::Point* transformed_point) {
  if (surface_id_.is_null())
    return surface_id_;
  cc::SurfaceHittest hittest(delegate, GetSurfaceManager());
  gfx::Transform target_transform;
  cc::SurfaceId target_surface_id =
      hittest.GetTargetSurfaceAtPoint(surface_id_, point, &target_transform);
  *transformed_point = point;
  if (!target_surface_id.is_null())
    target_transform.TransformPoint(transformed_point);
  return target_surface_id;
}

void DelegatedFrameHost::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    cc::SurfaceId original_surface,
    gfx::Point* transformed_point) {
  *transformed_point = point;
  if (surface_id_.is_null() || original_surface == surface_id_)
    return;

  gfx::Transform transform;
  cc::SurfaceHittest hittest(nullptr, GetSurfaceManager());
  if (hittest.GetTransformToTargetSurface(surface_id_, original_surface,
                                          &transform) &&
      transform.GetInverse(&transform)) {
    transform.TransformPoint(transformed_point);
  }
}

bool DelegatedFrameHost::ShouldSkipFrame(gfx::Size size_in_dip) const {
  // Should skip a frame only when another frame from the renderer is guaranteed
  // to replace it. Otherwise may cause hangs when the renderer is waiting for
  // the completion of latency infos (such as when taking a Snapshot.)
  if (can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT ||
      !resize_lock_.get())
    return false;

  return size_in_dip != resize_lock_->expected_size();
}

void DelegatedFrameHost::WasResized() {
  if (client_->DelegatedFrameHostDesiredSizeInDIP() !=
          current_frame_size_in_dip_ &&
      !client_->DelegatedFrameHostIsVisible())
    EvictDelegatedFrame();
  MaybeCreateResizeLock();
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
  if (!frame_subscriber() || !CanCopyToVideoFrame())
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
  if (!frame_subscriber()->ShouldCaptureFrame(damage_rect, present_time,
                                              &frame, &callback))
    return;

  // Get a texture to re-use; else, create a new one.
  scoped_refptr<OwnedMailbox> subscriber_texture;
  if (!idle_frame_subscriber_textures_.empty()) {
    subscriber_texture = idle_frame_subscriber_textures_.back();
    idle_frame_subscriber_textures_.pop_back();
  } else if (GLHelper* helper =
                 ImageTransportFactory::GetInstance()->GetGLHelper()) {
    subscriber_texture = new OwnedMailbox(helper);
  }

  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(),
          subscriber_texture,
          frame,
          base::Bind(callback, present_time)));
  // Setting the source in this copy request asks that the layer abort any prior
  // uncommitted copy requests made on behalf of the same frame subscriber.
  // This will not affect any of the copy requests spawned elsewhere from
  // DelegatedFrameHost (e.g., a call to CopyFromCompositingSurface() for
  // screenshots) since those copy requests do not specify |frame_subscriber()|
  // as a source.
  request->set_source(frame_subscriber());
  if (subscriber_texture.get()) {
    request->SetTextureMailbox(cc::TextureMailbox(
        subscriber_texture->mailbox(), subscriber_texture->sync_token(),
        subscriber_texture->target()));
  }

  if (surface_factory_.get()) {
    // To avoid unnecessary composites, go directly to the Surface rather than
    // through RequestCopyOfOutput (which goes through the browser
    // compositor).
    if (!request_copy_of_output_callback_for_testing_.is_null())
      request_copy_of_output_callback_for_testing_.Run(std::move(request));
    else
      surface_factory_->RequestCopyOfSurface(surface_id_, std::move(request));
  } else {
    request->set_area(gfx::Rect(current_frame_size_in_dip_));
    RequestCopyOfOutput(std::move(request));
  }
}

void DelegatedFrameHost::SwapDelegatedFrame(
    uint32_t output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  DCHECK(frame->delegated_frame_data.get());
  cc::DelegatedFrameData* frame_data = frame->delegated_frame_data.get();
  float frame_device_scale_factor = frame->metadata.device_scale_factor;

  DCHECK(!frame_data->render_pass_list.empty());

  cc::RenderPass* root_pass = frame_data->render_pass_list.back().get();

  gfx::Size frame_size = root_pass->output_rect.size();
  gfx::Size frame_size_in_dip =
      gfx::ConvertSizeToDIP(frame_device_scale_factor, frame_size);

  gfx::Rect damage_rect = root_pass->damage_rect;
  damage_rect.Intersect(gfx::Rect(frame_size));
  gfx::Rect damage_rect_in_dip =
      gfx::ConvertRectToDIP(frame_device_scale_factor, damage_rect);

  if (ShouldSkipFrame(frame_size_in_dip)) {
    cc::CompositorFrameAck ack;
    cc::TransferableResource::ReturnResources(frame_data->resource_list,
                                              &ack.resources);

    skipped_latency_info_list_.insert(skipped_latency_info_list_.end(),
                                      frame->metadata.latency_info.begin(),
                                      frame->metadata.latency_info.end());

    client_->DelegatedFrameHostSendCompositorSwapAck(output_surface_id, ack);
    skipped_frames_ = true;
    return;
  }

  if (skipped_frames_) {
    skipped_frames_ = false;
    damage_rect = gfx::Rect(frame_size);
    damage_rect_in_dip = gfx::Rect(frame_size_in_dip);

    // Give the same damage rect to the compositor.
    cc::RenderPass* root_pass = frame_data->render_pass_list.back().get();
    root_pass->damage_rect = damage_rect;
  }

  if (output_surface_id != last_output_surface_id_) {
    // Resource ids are scoped by the output surface.
    // If the originating output surface doesn't match the last one, it
    // indicates the renderer's output surface may have been recreated, in which
    // case we should recreate the DelegatedRendererLayer, to avoid matching
    // resources from the old one with resources from the new one which would
    // have the same id. Changing the layer to showing painted content destroys
    // the DelegatedRendererLayer.
    EvictDelegatedFrame();

    surface_factory_.reset();
    if (!surface_returned_resources_.empty())
      SendReturnedDelegatedResources(last_output_surface_id_);

    // Drop the cc::DelegatedFrameResourceCollection so that we will not return
    // any resources from the old output surface with the new output surface id.
    if (resource_collection_.get()) {
      resource_collection_->SetClient(NULL);

      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources(last_output_surface_id_);

      resource_collection_ = NULL;
    }
    last_output_surface_id_ = output_surface_id;
  }
  bool skip_frame = false;
  pending_delegated_ack_count_++;

  if (frame_size.IsEmpty()) {
    DCHECK(frame_data->resource_list.empty());
    EvictDelegatedFrame();
  } else {
    if (use_surfaces_) {
      ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
      cc::SurfaceManager* manager = factory->GetSurfaceManager();
      if (!surface_factory_) {
        surface_factory_ =
            make_scoped_ptr(new cc::SurfaceFactory(manager, this));
      }
      if (surface_id_.is_null() || frame_size != current_surface_size_ ||
          frame_size_in_dip != current_frame_size_in_dip_) {
        if (!surface_id_.is_null())
          surface_factory_->Destroy(surface_id_);
        surface_id_ = id_allocator_->GenerateId();
        surface_factory_->Create(surface_id_);
        // manager must outlive compositors using it.
        client_->DelegatedFrameHostGetLayer()->SetShowSurface(
            surface_id_,
            base::Bind(&SatisfyCallback, base::Unretained(manager)),
            base::Bind(&RequireCallback, base::Unretained(manager)), frame_size,
            frame_device_scale_factor, frame_size_in_dip);
        current_surface_size_ = frame_size;
        current_scale_factor_ = frame_device_scale_factor;
      }

      frame->metadata.latency_info.insert(frame->metadata.latency_info.end(),
                                          skipped_latency_info_list_.begin(),
                                          skipped_latency_info_list_.end());
      skipped_latency_info_list_.clear();

      gfx::Size desired_size = client_->DelegatedFrameHostDesiredSizeInDIP();
      if (desired_size != frame_size_in_dip && !desired_size.IsEmpty())
        skip_frame = true;

      cc::SurfaceFactory::DrawCallback ack_callback;
      if (compositor_ && !skip_frame) {
        ack_callback = base::Bind(&DelegatedFrameHost::SurfaceDrawn,
                                  AsWeakPtr(), output_surface_id);
      }
      surface_factory_->SubmitCompositorFrame(surface_id_, std::move(frame),
                                              ack_callback);
    } else {
      if (!resource_collection_.get()) {
        resource_collection_ = new cc::DelegatedFrameResourceCollection;
        resource_collection_->SetClient(this);
      }
      // If the physical frame size changes, we need a new |frame_provider_|. If
      // the physical frame size is the same, but the size in DIP changed, we
      // need to adjust the scale at which the frames will be drawn, and we do
      // this by making a new |frame_provider_| also to ensure the scale change
      // is presented in sync with the new frame content.
      if (!frame_provider_.get() ||
          frame_size != frame_provider_->frame_size() ||
          frame_size_in_dip != current_frame_size_in_dip_) {
        frame_provider_ = new cc::DelegatedFrameProvider(
            resource_collection_.get(), std::move(frame->delegated_frame_data));
        client_->DelegatedFrameHostGetLayer()->SetShowDelegatedContent(
            frame_provider_.get(), frame_size_in_dip);
      } else {
        frame_provider_->SetFrameData(std::move(frame->delegated_frame_data));
      }
    }
  }
  released_front_lock_ = NULL;
  current_frame_size_in_dip_ = frame_size_in_dip;
  CheckResizeLock();

  if (!damage_rect_in_dip.IsEmpty())
    client_->DelegatedFrameHostGetLayer()->OnDelegatedFrameDamage(
        damage_rect_in_dip);

  // Note that |compositor_| may be reset by SetShowSurface or
  // SetShowDelegatedContent above.
  if (!compositor_ || skip_frame) {
    SendDelegatedFrameAck(output_surface_id);
  } else if (!use_surfaces_) {
    std::vector<ui::LatencyInfo>::const_iterator it;
    for (it = frame->metadata.latency_info.begin();
         it != frame->metadata.latency_info.end(); ++it)
      compositor_->SetLatencyInfo(*it);
    // If we've previously skipped any latency infos add them.
    for (it = skipped_latency_info_list_.begin();
        it != skipped_latency_info_list_.end();
        ++it)
      compositor_->SetLatencyInfo(*it);
    skipped_latency_info_list_.clear();
    AddOnCommitCallbackAndDisableLocks(
        base::Bind(&DelegatedFrameHost::SendDelegatedFrameAck,
                   AsWeakPtr(), output_surface_id));
  } else {
    AddOnCommitCallbackAndDisableLocks(base::Closure());
  }
  // With Surfaces, WillDrawSurface() will be called as the trigger to attempt
  // a frame subscriber capture instead.
  if (!use_surfaces_)
    AttemptFrameSubscriberCapture(damage_rect);
  if (frame_provider_.get() || !surface_id_.is_null())
    delegated_frame_evictor_->SwappedFrame(
        client_->DelegatedFrameHostIsVisible());
  // Note: the frame may have been evicted immediately.
}

void DelegatedFrameHost::ClearDelegatedFrame() {
  if (frame_provider_.get() || !surface_id_.is_null())
    EvictDelegatedFrame();
}

void DelegatedFrameHost::SendDelegatedFrameAck(uint32_t output_surface_id) {
  cc::CompositorFrameAck ack;
  if (!surface_returned_resources_.empty())
    ack.resources.swap(surface_returned_resources_);
  if (resource_collection_.get())
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  client_->DelegatedFrameHostSendCompositorSwapAck(output_surface_id, ack);
  DCHECK_GT(pending_delegated_ack_count_, 0);
  pending_delegated_ack_count_--;
}

void DelegatedFrameHost::SurfaceDrawn(uint32_t output_surface_id,
                                      cc::SurfaceDrawStatus drawn) {
  SendDelegatedFrameAck(output_surface_id);
}

void DelegatedFrameHost::UnusedResourcesAreAvailable() {
  if (pending_delegated_ack_count_)
    return;

  SendReturnedDelegatedResources(last_output_surface_id_);
}

void DelegatedFrameHost::SendReturnedDelegatedResources(
    uint32_t output_surface_id) {
  cc::CompositorFrameAck ack;
  if (!surface_returned_resources_.empty()) {
    ack.resources.swap(surface_returned_resources_);
  } else {
    DCHECK(resource_collection_.get());
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  }
  DCHECK(!ack.resources.empty());

  client_->DelegatedFrameHostSendReclaimCompositorResources(output_surface_id,
                                                            ack);
}

void DelegatedFrameHost::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;
  std::copy(resources.begin(),
            resources.end(),
            std::back_inserter(surface_returned_resources_));
  if (!pending_delegated_ack_count_)
    SendReturnedDelegatedResources(last_output_surface_id_);
}

void DelegatedFrameHost::WillDrawSurface(cc::SurfaceId id,
                                         const gfx::Rect& damage_rect) {
  if (id != surface_id_)
    return;
  AttemptFrameSubscriberCapture(damage_rect);
}

void DelegatedFrameHost::SetBeginFrameSource(
    cc::SurfaceId surface_id,
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Hook this up.
}

void DelegatedFrameHost::EvictDelegatedFrame() {
  client_->DelegatedFrameHostGetLayer()->SetShowSolidColorContent();
  frame_provider_ = NULL;
  if (!surface_id_.is_null()) {
    surface_factory_->Destroy(surface_id_);
    surface_id_ = cc::SurfaceId();
  }
  delegated_frame_evictor_->DiscardedFrame();
}

// static
void DelegatedFrameHost::CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const ReadbackRequestCallback& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty()) {
    callback.Run(SkBitmap(), content::READBACK_FAILED);
    return;
  }

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = result->size();
  else
    output_size_in_pixel = dst_size_in_pixel;

  if (result->HasTexture()) {
    // GPU-accelerated path
    PrepareTextureCopyOutputResult(output_size_in_pixel, color_type, callback,
                                   std::move(result));
    return;
  }

  DCHECK(result->HasBitmap());
  // Software path
  PrepareBitmapCopyOutputResult(output_size_in_pixel, color_type, callback,
                                std::move(result));
}

static void CopyFromCompositingSurfaceFinished(
    const ReadbackRequestCallback& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  bitmap_pixels_lock.reset();

  gpu::SyncToken sync_token;
  if (result) {
    GLHelper* gl_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
    if (gl_helper)
      gl_helper->GenerateSyncToken(&sync_token);
  }
  const bool lost_resource = !sync_token.HasData();
  release_callback->Run(sync_token, lost_resource);

  callback.Run(*bitmap,
               result ? content::READBACK_SUCCESS : content::READBACK_FAILED);
}

// static
void DelegatedFrameHost::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const ReadbackRequestCallback& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK(result->HasTexture());
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, SkBitmap(), content::READBACK_FAILED));

  // TODO(siva.gunturi): We should be able to validate the format here using
  // GLHelper::IsReadbackConfigSupported before we processs the result.
  // See crbug.com/415682 and crbug.com/415131.
  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!bitmap->tryAllocPixels(SkImageInfo::Make(
          dst_size_in_pixel.width(), dst_size_in_pixel.height(), color_type,
          kOpaque_SkAlphaType))) {
    scoped_callback_runner.Reset(base::Bind(
        callback, SkBitmap(), content::READBACK_BITMAP_ALLOCATION_FAILURE));
    return;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8_t* pixels = static_cast<uint8_t*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(), result->size(),
      gfx::Rect(result->size()), dst_size_in_pixel, pixels, color_type,
      base::Bind(&CopyFromCompositingSurfaceFinished, callback,
                 base::Passed(&release_callback), base::Passed(&bitmap),
                 base::Passed(&bitmap_pixels_lock)),
      GLHelper::SCALER_QUALITY_GOOD);
}

// static
void DelegatedFrameHost::PrepareBitmapCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType preferred_color_type,
    const ReadbackRequestCallback& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  SkColorType color_type = preferred_color_type;
  if (color_type != kN32_SkColorType && color_type != kAlpha_8_SkColorType) {
    // Switch back to default colortype if format not supported.
    color_type = kN32_SkColorType;
  }
  DCHECK(result->HasBitmap());
  scoped_ptr<SkBitmap> source = result->TakeBitmap();
  DCHECK(source);
  SkBitmap scaled_bitmap;
  if (source->width() != dst_size_in_pixel.width() ||
      source->height() != dst_size_in_pixel.height()) {
    scaled_bitmap =
        skia::ImageOperations::Resize(*source,
                                      skia::ImageOperations::RESIZE_BEST,
                                      dst_size_in_pixel.width(),
                                      dst_size_in_pixel.height());
  } else {
    scaled_bitmap = *source;
  }
  if (color_type == kN32_SkColorType) {
    DCHECK_EQ(scaled_bitmap.colorType(), kN32_SkColorType);
    callback.Run(scaled_bitmap, READBACK_SUCCESS);
    return;
  }
  DCHECK_EQ(color_type, kAlpha_8_SkColorType);
  // The software path currently always returns N32 bitmap regardless of the
  // |color_type| we ask for.
  DCHECK_EQ(scaled_bitmap.colorType(), kN32_SkColorType);
  // Paint |scaledBitmap| to alpha-only |grayscale_bitmap|.
  SkBitmap grayscale_bitmap;
  bool success = grayscale_bitmap.tryAllocPixels(
      SkImageInfo::MakeA8(scaled_bitmap.width(), scaled_bitmap.height()));
  if (!success) {
    callback.Run(SkBitmap(), content::READBACK_BITMAP_ALLOCATION_FAILURE);
    return;
  }
  SkCanvas canvas(grayscale_bitmap);
  SkPaint paint;
  skia::RefPtr<SkColorFilter> filter =
      skia::AdoptRef(SkLumaColorFilter::Create());
  paint.setColorFilter(filter.get());
  canvas.drawBitmap(scaled_bitmap, SkIntToScalar(0), SkIntToScalar(0), &paint);
  callback.Run(grayscale_bitmap, READBACK_SUCCESS);
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
    base::WeakPtr<DelegatedFrameHost> dfh,
    const base::Callback<void(bool)>& callback,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  callback.Run(result);

  gpu::SyncToken sync_token;
  if (result) {
    GLHelper* gl_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
    gl_helper->GenerateSyncToken(&sync_token);
  }
  if (release_callback) {
    // A release callback means the texture came from the compositor, so there
    // should be no |subscriber_texture|.
    DCHECK(!subscriber_texture.get());
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
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, gfx::Rect(), false));
  base::ScopedClosureRunner scoped_return_subscriber_texture(
      base::Bind(&ReturnSubscriberTexture, dfh, subscriber_texture,
      gpu::SyncToken()));

  if (!dfh)
    return;
  if (result->IsEmpty())
    return;
  if (result->size().IsEmpty())
    return;

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's visible_rect() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
      video_frame->visible_rect(), result->size());
  region_in_frame = gfx::Rect(region_in_frame.x() & ~1,
                              region_in_frame.y() & ~1,
                              region_in_frame.width() & ~1,
                              region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return;

  if (!result->HasTexture()) {
    DCHECK(result->HasBitmap());
    scoped_ptr<SkBitmap> bitmap = result->TakeBitmap();
    // Scale the bitmap to the required size, if necessary.
    SkBitmap scaled_bitmap;
    if (result->size() != region_in_frame.size()) {
      skia::ImageOperations::ResizeMethod method =
          skia::ImageOperations::RESIZE_GOOD;
      scaled_bitmap = skia::ImageOperations::Resize(*bitmap.get(), method,
                                                    region_in_frame.width(),
                                                    region_in_frame.height());
    } else {
      scaled_bitmap = *bitmap.get();
    }

    {
      SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

      media::CopyRGBToVideoFrame(
          reinterpret_cast<uint8_t*>(scaled_bitmap.getPixels()),
          scaled_bitmap.rowBytes(), region_in_frame, video_frame.get());
    }
    ignore_result(scoped_callback_runner.Release());
    callback.Run(region_in_frame, true);
    return;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;
  if (subscriber_texture.get() && !subscriber_texture->texture_id())
    return;

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());

  gfx::Rect result_rect(result->size());

  content::ReadbackYUVInterface* yuv_readback_pipeline =
      dfh->yuv_readback_pipeline_.get();
  if (yuv_readback_pipeline == NULL ||
      yuv_readback_pipeline->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline->scaler()->DstSize() != region_in_frame.size()) {
    GLHelper::ScalerQuality quality = GLHelper::SCALER_QUALITY_FAST;
    std::string quality_switch = switches::kTabCaptureDownscaleQuality;
    // If we're scaling up, we can use the "best" quality.
    if (result_rect.size().width() < region_in_frame.size().width() &&
        result_rect.size().height() < region_in_frame.size().height())
      quality_switch = switches::kTabCaptureUpscaleQuality;

    std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            quality_switch);
    if (switch_value == "fast")
      quality = GLHelper::SCALER_QUALITY_FAST;
    else if (switch_value == "good")
      quality = GLHelper::SCALER_QUALITY_GOOD;
    else if (switch_value == "best")
      quality = GLHelper::SCALER_QUALITY_BEST;

    dfh->yuv_readback_pipeline_.reset(
        gl_helper->CreateReadbackPipelineYUV(quality,
                                             result_rect.size(),
                                             result_rect,
                                             region_in_frame.size(),
                                             true,
                                             true));
    yuv_readback_pipeline = dfh->yuv_readback_pipeline_.get();
  }

  ignore_result(scoped_callback_runner.Release());
  ignore_result(scoped_return_subscriber_texture.Release());

  base::Callback<void(bool result)> finished_callback = base::Bind(
      &DelegatedFrameHost::CopyFromCompositingSurfaceFinishedForVideo,
      dfh->AsWeakPtr(), base::Bind(callback, region_in_frame),
      subscriber_texture, base::Passed(&release_callback));
  yuv_readback_pipeline->ReadbackYUV(texture_mailbox.mailbox(),
                                     texture_mailbox.sync_token(),
                                     video_frame.get(),
                                     region_in_frame.origin(),
                                     finished_callback);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::CompositorObserver implementation:

void DelegatedFrameHost::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  if (can_lock_compositor_ == NO_PENDING_COMMIT) {
    can_lock_compositor_ = YES_CAN_LOCK;
    if (resize_lock_.get() && resize_lock_->GrabDeferredLock())
      can_lock_compositor_ = YES_DID_LOCK;
  }
  RunOnCommitCallbacks();
  if (resize_lock_ &&
      resize_lock_->expected_size() == current_frame_size_in_dip_) {
    resize_lock_.reset();
    client_->DelegatedFrameHostResizeLockWasReleased();
    // We may have had a resize while we had the lock (e.g. if the lock expired,
    // or if the UI still gave us some resizes), so make sure we grab a new lock
    // if necessary.
    MaybeCreateResizeLock();
  }
}

void DelegatedFrameHost::OnCompositingStarted(
    ui::Compositor* compositor, base::TimeTicks start_time) {
  last_draw_ended_ = start_time;
}

void DelegatedFrameHost::OnCompositingEnded(
    ui::Compositor* compositor) {
}

void DelegatedFrameHost::OnCompositingAborted(ui::Compositor* compositor) {
}

void DelegatedFrameHost::OnCompositingLockStateChanged(
    ui::Compositor* compositor) {
  // A compositor lock that is part of a resize lock timed out. We
  // should display a renderer frame.
  if (!compositor->IsLocked() && can_lock_compositor_ == YES_DID_LOCK) {
    can_lock_compositor_ = NO_PENDING_RENDERER_FRAME;
  }
}

void DelegatedFrameHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor, compositor_);
  ResetCompositor();
  DCHECK(!compositor_);
}

void DelegatedFrameHost::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  SetVSyncParameters(timebase, interval);
  if (client_->DelegatedFrameHostIsVisible())
    client_->DelegatedFrameHostUpdateVSyncParameters(timebase, interval);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ImageTransportFactoryObserver implementation:

void DelegatedFrameHost::OnLostResources() {
  if (frame_provider_.get() || !surface_id_.is_null())
    EvictDelegatedFrame();
  idle_frame_subscriber_textures_.clear();
  yuv_readback_pipeline_.reset();

  client_->DelegatedFrameHostOnLostCompositorResources();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

DelegatedFrameHost::~DelegatedFrameHost() {
  DCHECK(!compositor_);
  ImageTransportFactory::GetInstance()->RemoveObserver(this);

  if (!surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
  if (resource_collection_.get())
    resource_collection_->SetClient(NULL);

  DCHECK(!vsync_manager_.get());
}

void DelegatedFrameHost::RunOnCommitCallbacks() {
  for (std::vector<base::Closure>::const_iterator
      it = on_compositing_did_commit_callbacks_.begin();
      it != on_compositing_did_commit_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_did_commit_callbacks_.clear();
}

void DelegatedFrameHost::AddOnCommitCallbackAndDisableLocks(
    const base::Closure& callback) {
  DCHECK(compositor_);

  can_lock_compositor_ = NO_PENDING_COMMIT;
  if (!callback.is_null())
    on_compositing_did_commit_callbacks_.push_back(callback);
}

void DelegatedFrameHost::SetCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
  if (!compositor)
    return;
  compositor_ = compositor;
  compositor_->AddObserver(this);
  DCHECK(!vsync_manager_.get());
  vsync_manager_ = compositor_->vsync_manager();
  vsync_manager_->AddObserver(this);
}

void DelegatedFrameHost::ResetCompositor() {
  if (!compositor_)
    return;
  RunOnCommitCallbacks();
  if (resize_lock_) {
    resize_lock_.reset();
    client_->DelegatedFrameHostResizeLockWasReleased();
  }
  if (compositor_->HasObserver(this))
    compositor_->RemoveObserver(this);
  if (vsync_manager_.get()) {
    vsync_manager_->RemoveObserver(this);
    vsync_manager_ = NULL;
  }
  compositor_ = nullptr;
}

void DelegatedFrameHost::SetVSyncParameters(const base::TimeTicks& timebase,
                                            const base::TimeDelta& interval) {
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
}

void DelegatedFrameHost::LockResources() {
  DCHECK(frame_provider_.get() || !surface_id_.is_null());
  delegated_frame_evictor_->LockFrame();
}

void DelegatedFrameHost::RequestCopyOfOutput(
    scoped_ptr<cc::CopyOutputRequest> request) {
  if (!request_copy_of_output_callback_for_testing_.is_null())
    request_copy_of_output_callback_for_testing_.Run(std::move(request));
  else
    client_->DelegatedFrameHostGetLayer()->RequestCopyOfOutput(
        std::move(request));
}

void DelegatedFrameHost::UnlockResources() {
  DCHECK(frame_provider_.get() || !surface_id_.is_null());
  delegated_frame_evictor_->UnlockFrame();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::LayerOwnerDelegate implementation:

void DelegatedFrameHost::OnLayerRecreated(ui::Layer* old_layer,
                                                ui::Layer* new_layer) {
  // The new_layer is the one that will be used by our Window, so that's the one
  // that should keep our frame. old_layer will be returned to the
  // RecreateLayer caller, and should have a copy.
  if (frame_provider_.get()) {
    new_layer->SetShowDelegatedContent(frame_provider_.get(),
                                       current_frame_size_in_dip_);
  }
  if (!surface_id_.is_null()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    cc::SurfaceManager* manager = factory->GetSurfaceManager();
    new_layer->SetShowSurface(
        surface_id_, base::Bind(&SatisfyCallback, base::Unretained(manager)),
        base::Bind(&RequireCallback, base::Unretained(manager)),
        current_surface_size_, current_scale_factor_,
        current_frame_size_in_dip_);
  }
}

}  // namespace content
