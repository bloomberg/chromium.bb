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
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_hittest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/compositor_resize_lock.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost

DelegatedFrameHost::DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                                       DelegatedFrameHostClient* client,
                                       bool enable_surface_synchronization,
                                       bool enable_viz,
                                       bool should_register_frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      client_(client),
      enable_surface_synchronization_(enable_surface_synchronization),
      enable_viz_(enable_viz),
      should_register_frame_sink_id_(should_register_frame_sink_id),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->AddObserver(this);
  viz::HostFrameSinkManager* host_frame_sink_manager =
      factory->GetContextFactoryPrivate()->GetHostFrameSinkManager();
  host_frame_sink_manager->RegisterFrameSinkId(frame_sink_id_, this);
  host_frame_sink_manager->EnableSynchronizationReporting(
      frame_sink_id_, "Compositing.MainFrameSynchronization.Duration");
  host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_,
                                                  "DelegatedFrameHost");
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
}

void DelegatedFrameHost::WasShown(
    const viz::LocalSurfaceId& new_pending_local_surface_id,
    const gfx::Size& new_pending_dip_size,
    const ui::LatencyInfo& latency_info) {
  frame_evictor_->SetVisible(true);

  if (!enable_surface_synchronization_ && !HasFallbackSurface() &&
      !released_front_lock_.get()) {
    if (compositor_)
      released_front_lock_ = compositor_->GetCompositorLock(nullptr);
  }

  if (compositor_)
    compositor_->SetLatencyInfo(latency_info);

  // Use the default deadline to synchronize web content with browser UI.
  // TODO(fsamuel): Investigate if there is a better deadline to use here.
  WasResized(new_pending_local_surface_id, new_pending_dip_size,
             cc::DeadlinePolicy::UseDefaultDeadline());
}

bool DelegatedFrameHost::HasSavedFrame() const {
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

  if (!HasFallbackSurface())
    return;

  if (!client_->DelegatedFrameCanCreateResizeLock())
    return;

  gfx::Size desired_size = pending_surface_dip_size_;
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
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (!CanCopyFromCompositingSurface() ||
      current_frame_size_in_dip_.IsEmpty()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                std::move(callback).Run(result->AsSkBitmap());
              },
              std::move(callback)));

  if (src_subrect.IsEmpty()) {
    request->set_area(gfx::Rect(current_frame_size_in_dip_));
  } else {
    request->set_area(src_subrect);
  }

  // If VIZ display compositing is disabled, the request will be issued directly
  // to CompositorFrameSinkSupport, which requires Surface pixel coordinates.
  if (!enable_viz_) {
    request->set_area(
        gfx::ScaleToRoundedRect(request->area(), active_device_scale_factor_));
  }

  if (!output_size.IsEmpty()) {
    request->set_result_selection(gfx::Rect(output_size));
    request->SetScaleRatio(
        gfx::Vector2d(request->area().width(), request->area().height()),
        gfx::Vector2d(output_size.width(), output_size.height()));
  }

  if (enable_viz_) {
    client_->DelegatedFrameHostGetLayer()->RequestCopyOfOutput(
        std::move(request));
  } else {
    support_->RequestCopyOfSurface(std::move(request));
  }
}

bool DelegatedFrameHost::CanCopyFromCompositingSurface() const {
  return (enable_viz_ || support_) && HasFallbackSurface() &&
         active_device_scale_factor_ != 0.f;
}

bool DelegatedFrameHost::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  viz::SurfaceId surface_id(frame_sink_id_, active_local_surface_id_);
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
  if (!HasFallbackSurface())
    return false;

  return target_view->TransformPointToLocalCoordSpace(
      point, viz::SurfaceId(frame_sink_id_, active_local_surface_id_),
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

void DelegatedFrameHost::SetWantsAnimateOnlyBeginFrames() {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  support_->SetWantsAnimateOnlyBeginFrames();
}

void DelegatedFrameHost::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
  if (enable_viz_) {
    NOTIMPLEMENTED();
    return;
  }

  DCHECK(!ack.has_damage);
  support_->DidNotProduceFrame(ack);
}

bool DelegatedFrameHost::HasPrimarySurface() const {
  const viz::SurfaceId* primary_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetPrimarySurfaceId();
  return primary_surface_id && primary_surface_id->is_valid();
}

bool DelegatedFrameHost::HasFallbackSurface() const {
  const viz::SurfaceId* fallback_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetFallbackSurfaceId();
  return fallback_surface_id && fallback_surface_id->is_valid();
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

void DelegatedFrameHost::WasResized(
    const viz::LocalSurfaceId& new_pending_local_surface_id,
    const gfx::Size& new_pending_dip_size,
    cc::DeadlinePolicy deadline_policy) {
  const viz::SurfaceId* primary_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetPrimarySurfaceId();

  pending_local_surface_id_ = new_pending_local_surface_id;
  pending_surface_dip_size_ = new_pending_dip_size;

  if (enable_surface_synchronization_) {
    if (!client_->DelegatedFrameHostIsVisible()) {
      if (HasFallbackSurface()) {
        client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
            viz::SurfaceId());
      }
      return;
    }

    if (!primary_surface_id ||
        primary_surface_id->local_surface_id() != pending_local_surface_id_) {
      current_frame_size_in_dip_ = pending_surface_dip_size_;

      viz::SurfaceId surface_id(frame_sink_id_, pending_local_surface_id_);
#if defined(OS_WIN) || defined(OS_LINUX)
      // On Windows and Linux, we would like to produce new content as soon as
      // possible or the OS will create an additional black gutter. Until we can
      // block resize on surface synchronization on these platforms, we will not
      // block UI on the top-level renderer.
      deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
#endif
      client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
          surface_id, current_frame_size_in_dip_, GetGutterColor(),
          deadline_policy);
      if (compositor_ && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                             switches::kDisableResizeLock)) {
        compositor_->OnChildResizing();
      }
    }

    // Input throttling and guttering are handled differently when surface
    // synchronization is enabled so exit early here.
    return;
  }

  if (pending_surface_dip_size_ != current_frame_size_in_dip_ &&
      !client_->DelegatedFrameHostIsVisible()) {
    EvictDelegatedFrame();
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
  return client_->DelegatedFrameHostGetGutterColor();
}

void DelegatedFrameHost::UpdateGutters() {
  if (!HasFallbackSurface() || enable_surface_synchronization_) {
    right_gutter_.reset();
    bottom_gutter_.reset();
    return;
  }

  gfx::Size size_in_dip = pending_surface_dip_size_;
  if (current_frame_size_in_dip_.width() < size_in_dip.width()) {
    right_gutter_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    right_gutter_->SetColor(GetGutterColor());
    int width = size_in_dip.width() - current_frame_size_in_dip_.width();
    // The right gutter also includes the bottom-right corner, if necessary.
    int height = size_in_dip.height();
    right_gutter_->SetBounds(
        gfx::Rect(current_frame_size_in_dip_.width(), 0, width, height));

    client_->DelegatedFrameHostGetLayer()->Add(right_gutter_.get());
  } else {
    right_gutter_.reset();
  }

  if (current_frame_size_in_dip_.height() < size_in_dip.height()) {
    bottom_gutter_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    bottom_gutter_->SetColor(GetGutterColor());
    int width = current_frame_size_in_dip_.width();
    int height = size_in_dip.height() - current_frame_size_in_dip_.height();
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
    return pending_surface_dip_size_;
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
    root_pass->damage_rect = gfx::Rect(frame_size);
  }

  frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                     skipped_latency_info_list_.begin(),
                                     skipped_latency_info_list_.end());
  skipped_latency_info_list_.clear();

  // If surface synchronization is off, then OnFirstSurfaceActivation will be
  // called in the same call stack.
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  std::move(hit_test_region_list));
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
  if (enable_surface_synchronization_) {
    // If this is the first Surface created after navigation, notify |client_|.
    // If the Surface was created before navigation, drop it.
    uint32_t parent_sequence_number =
        surface_info.id().local_surface_id().parent_sequence_number();
    uint32_t latest_parent_sequence_number =
        pending_local_surface_id_.parent_sequence_number();
    // If |latest_parent_sequence_number| is less than
    // |first_parent_sequence_number_after_navigation_|, then the parent id has
    // wrapped around. Make sure that case is covered.
    if (parent_sequence_number >=
            first_parent_sequence_number_after_navigation_ ||
        (latest_parent_sequence_number <
             first_parent_sequence_number_after_navigation_ &&
         parent_sequence_number <= latest_parent_sequence_number)) {
      if (!received_frame_after_navigation_) {
        received_frame_after_navigation_ = true;
        client_->DidReceiveFirstFrameAfterNavigation();
      }
    } else {
      ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
      viz::HostFrameSinkManager* host_frame_sink_manager =
          factory->GetContextFactoryPrivate()->GetHostFrameSinkManager();
      host_frame_sink_manager->DropTemporaryReference(surface_info.id());
      return;
    }

    // If there's no primary surface, then we don't wish to display content at
    // this time (e.g. the view is hidden) and so we don't need a fallback
    // surface either. Since we won't use the fallback surface, we drop the
    // temporary reference here to save resources.
    if (!HasPrimarySurface()) {
      ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
      viz::HostFrameSinkManager* host_frame_sink_manager =
          factory->GetContextFactoryPrivate()->GetHostFrameSinkManager();
      host_frame_sink_manager->DropTemporaryReference(surface_info.id());
      return;
    }
  } else {
    client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
        surface_info.id(), frame_size_in_dip, GetGutterColor(),
        cc::DeadlinePolicy::UseDefaultDeadline());
  }

  client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
      surface_info.id());
  active_local_surface_id_ = surface_info.id().local_surface_id();
  active_device_scale_factor_ = surface_info.device_scale_factor();

  // Surface synchronization deals with resizes in WasResized().
  if (!enable_surface_synchronization_) {
    released_front_lock_ = nullptr;
    current_frame_size_in_dip_ = frame_size_in_dip;
    CheckResizeLock();

    UpdateGutters();
  }

  // This is used by macOS' unique resize path.
  client_->OnFirstSurfaceActivation(surface_info);

  frame_evictor_->SwappedFrame(client_->DelegatedFrameHostIsVisible());
  // Note: the frame may have been evicted immediately.
}

void DelegatedFrameHost::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

void DelegatedFrameHost::OnBeginFrame(const viz::BeginFrameArgs& args) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFrame(args);
  client_->OnBeginFrame(args.frame_time);
}

void DelegatedFrameHost::EvictDelegatedFrame() {
  if (!HasSavedFrame())
    return;

  std::vector<viz::SurfaceId> surface_ids = {GetCurrentSurfaceId()};

  GetHostFrameSinkManager()->EvictSurfaces(surface_ids);

  if (enable_surface_synchronization_) {
    if (HasFallbackSurface()) {
      client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
          viz::SurfaceId());
    }
  } else {
    client_->DelegatedFrameHostGetLayer()->SetShowSolidColorContent();
    resize_lock_.reset();
    UpdateGutters();
  }

  frame_evictor_->DiscardedFrame();
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
                                              base::TimeTicks start_time) {}

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

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ImageTransportFactoryObserver implementation:

void DelegatedFrameHost::OnLostResources() {
  EvictDelegatedFrame();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

void DelegatedFrameHost::SetCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
  if (!compositor)
    return;
  compositor_ = compositor;
  compositor_->AddObserver(this);
  if (should_register_frame_sink_id_)
    compositor_->AddFrameSink(frame_sink_id_);
}

void DelegatedFrameHost::ResetCompositor() {
  if (!compositor_)
    return;
  resize_lock_.reset();
  if (compositor_->HasObserver(this))
    compositor_->RemoveObserver(this);
  if (should_register_frame_sink_id_)
    compositor_->RemoveFrameSink(frame_sink_id_);
  compositor_ = nullptr;
}

void DelegatedFrameHost::LockResources() {
  DCHECK(active_local_surface_id_.is_valid());
  frame_evictor_->LockFrame();
}

void DelegatedFrameHost::UnlockResources() {
  DCHECK(active_local_surface_id_.is_valid());
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
  if (compositor_ && should_register_frame_sink_id_)
    compositor_->AddFrameSink(frame_sink_id_);
  if (needs_begin_frame_)
    support_->SetNeedsBeginFrame(true);
}

void DelegatedFrameHost::ResetCompositorFrameSinkSupport() {
  if (!support_)
    return;
  if (compositor_ && should_register_frame_sink_id_)
    compositor_->RemoveFrameSink(frame_sink_id_);
  support_.reset();
}

void DelegatedFrameHost::DidNavigate() {
  first_parent_sequence_number_after_navigation_ =
      pending_local_surface_id_.parent_sequence_number();
  received_frame_after_navigation_ = false;
}

bool DelegatedFrameHost::IsPrimarySurfaceEvicted() const {
  return active_local_surface_id_ == pending_local_surface_id_ &&
         !HasSavedFrame();
}

}  // namespace content
