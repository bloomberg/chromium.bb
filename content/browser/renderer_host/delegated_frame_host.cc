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
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
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
#include "content/common/tab_switching_time_callback.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost

DelegatedFrameHost::DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                                       DelegatedFrameHostClient* client,
                                       bool should_register_frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      client_(client),
      enable_viz_(
          base::FeatureList::IsEnabled(features::kVizDisplayCompositor)),
      should_register_frame_sink_id_(should_register_frame_sink_id),
      host_frame_sink_manager_(GetHostFrameSinkManager()),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->AddObserver(this);
  DCHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->RegisterFrameSinkId(frame_sink_id_, this);
  host_frame_sink_manager_->EnableSynchronizationReporting(
      frame_sink_id_, "Compositing.MainFrameSynchronization.Duration");
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "DelegatedFrameHost");
  CreateCompositorFrameSinkSupport();
}

DelegatedFrameHost::~DelegatedFrameHost() {
  DCHECK(!compositor_);
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->RemoveObserver(this);

  ResetCompositorFrameSinkSupport();
  DCHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void DelegatedFrameHost::WasShown(
    const viz::LocalSurfaceId& new_pending_local_surface_id,
    const gfx::Size& new_pending_dip_size,
    bool record_presentation_time) {
  frame_evictor_->SetVisible(true);
  if (record_presentation_time && compositor_) {
    compositor_->RequestPresentationTimeForNextFrame(
        CreateTabSwitchingTimeRecorder(base::TimeTicks::Now()));
  }

  // Use the default deadline to synchronize web content with browser UI.
  // TODO(fsamuel): Investigate if there is a better deadline to use here.
  EmbedSurface(new_pending_local_surface_id, new_pending_dip_size,
               cc::DeadlinePolicy::UseDefaultDeadline());
}

bool DelegatedFrameHost::HasSavedFrame() const {
  return frame_evictor_->HasFrame();
}

void DelegatedFrameHost::WasHidden() {
  frame_evictor_->SetVisible(false);
}

void DelegatedFrameHost::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                std::move(callback).Run(result->AsSkBitmap());
              },
              std::move(callback)));

  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);
  if (!output_size.IsEmpty())
    request->set_result_selection(gfx::Rect(output_size));

  // If there is enough information to populate the copy output request fields,
  // then process it now. Otherwise, wait until the information becomes
  // available.
  if (CanCopyFromCompositingSurface() &&
      active_local_surface_id_ == pending_local_surface_id_)
    ProcessCopyOutputRequest(std::move(request));
  else
    pending_first_frame_requests_.push_back(std::move(request));
}

void DelegatedFrameHost::ProcessCopyOutputRequest(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  if (!request->has_area())
    request->set_area(gfx::Rect(pending_surface_dip_size_));

  request->set_area(
      gfx::ScaleToRoundedRect(request->area(), active_device_scale_factor_));

  if (request->has_result_selection()) {
    const gfx::Rect& area = request->area();
    const gfx::Rect& result_selection = request->result_selection();
    if (area.IsEmpty() || result_selection.IsEmpty()) {
      // Viz would normally return an empty result for an empty selection.
      // However, this guard here is still necessary to protect against setting
      // an illegal scaling ratio.
      return;
    }
    request->SetScaleRatio(
        gfx::Vector2d(area.width(), area.height()),
        gfx::Vector2d(result_selection.width(), result_selection.height()));
  }
  DCHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->RequestCopyOfOutput(
      viz::SurfaceId(frame_sink_id_, pending_local_surface_id_),
      std::move(request));
}

bool DelegatedFrameHost::CanCopyFromCompositingSurface() const {
  return HasFallbackSurface() && active_device_scale_factor_ != 0.f;
}

bool DelegatedFrameHost::TransformPointToLocalCoordSpaceLegacy(
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

void DelegatedFrameHost::EmbedSurface(
    const viz::LocalSurfaceId& new_pending_local_surface_id,
    const gfx::Size& new_pending_dip_size,
    cc::DeadlinePolicy deadline_policy) {
  const viz::SurfaceId* primary_surface_id =
      client_->DelegatedFrameHostGetLayer()->GetPrimarySurfaceId();

  pending_local_surface_id_ = new_pending_local_surface_id;
  pending_surface_dip_size_ = new_pending_dip_size;

  if (!client_->DelegatedFrameHostIsVisible()) {
    // If the tab is resized while hidden, reset the fallback so that the next
    // time user switches back to it the page is blank. This is preferred to
    // showing contents of old size. Don't call EvictDelegatedFrame to avoid
    // races when dragging tabs across displays. See https://crbug.com/813157.
    if (pending_surface_dip_size_ != current_frame_size_in_dip_ &&
        HasFallbackSurface()) {
      client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
          viz::SurfaceId());
    }
    // Don't update the SurfaceLayer when invisible to avoid blocking on
    // renderers that do not submit CompositorFrames. Next time the renderer
    // is visible, EmbedSurface will be called again. See WasShown.
    return;
  }

  if (!primary_surface_id ||
      primary_surface_id->local_surface_id() != pending_local_surface_id_) {
    viz::SurfaceId surface_id(frame_sink_id_, pending_local_surface_id_);
#if defined(OS_WIN) || defined(USE_X11)
    // On Windows and Linux, we would like to produce new content as soon as
    // possible or the OS will create an additional black gutter. Until we can
    // block resize on surface synchronization on these platforms, we will not
    // block UI on the top-level renderer. The exception to this is if we're
    // using an infinite deadline, in which case we should respect the
    // specified deadline and block UI since that's what was requested.
    if (deadline_policy.policy_type() !=
            cc::DeadlinePolicy::kUseInfiniteDeadline &&
        !current_frame_size_in_dip_.IsEmpty() &&
        current_frame_size_in_dip_ != pending_surface_dip_size_) {
      deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
    }
#endif
    current_frame_size_in_dip_ = pending_surface_dip_size_;
    client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
        surface_id, current_frame_size_in_dip_, GetGutterColor(),
        deadline_policy, false /* stretch_content_to_fill_bounds */);
    if (compositor_ && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                           switches::kDisableResizeLock)) {
      compositor_->OnChildResizing();
    }
  }
}

SkColor DelegatedFrameHost::GetGutterColor() const {
  // In fullscreen mode resizing is uncommon, so it makes more sense to
  // make the initial switch to fullscreen mode look better by using black as
  // the gutter color.
  return client_->DelegatedFrameHostGetGutterColor();
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
    base::Optional<viz::HitTestRegionList> hit_test_region_list) {
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
  EvictDelegatedFrame();
}

void DelegatedFrameHost::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);
}

void DelegatedFrameHost::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  renderer_compositor_frame_sink_->DidPresentCompositorFrame(presentation_token,
                                                             feedback);
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
  // If there's no primary surface, then we don't wish to display content at
  // this time (e.g. the view is hidden) and so we don't need a fallback
  // surface either. Since we won't use the fallback surface, we drop the
  // temporary reference here to save resources.
  if (!HasPrimarySurface()) {
    DCHECK(host_frame_sink_manager_);
    host_frame_sink_manager_->DropTemporaryReference(surface_info.id());
    return;
  }

  client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
      surface_info.id());
  active_local_surface_id_ = surface_info.id().local_surface_id();
  active_device_scale_factor_ = surface_info.device_scale_factor();

  // This is used by macOS' unique resize path.
  client_->OnFirstSurfaceActivation(surface_info);

  frame_evictor_->SwappedFrame(client_->DelegatedFrameHostIsVisible());
  // Note: the frame may have been evicted immediately.

  if (!pending_first_frame_requests_.empty()) {
    DCHECK(CanCopyFromCompositingSurface());
    for (auto& request : pending_first_frame_requests_)
      ProcessCopyOutputRequest(std::move(request));
    pending_first_frame_requests_.clear();
  }
}

void DelegatedFrameHost::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

void DelegatedFrameHost::OnBeginFrame(const viz::BeginFrameArgs& args) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFrame(args);
  client_->OnBeginFrame(args.frame_time);
}

void DelegatedFrameHost::ResetFallbackToFirstNavigationSurface() {
  if (HasFallbackSurface()) {
    client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(viz::SurfaceId(
        frame_sink_id_, first_local_surface_id_after_navigation_));
  }
}

void DelegatedFrameHost::EvictDelegatedFrame() {
  // Reset fallback and primary surfaces.
  if (HasFallbackSurface()) {
    client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
        viz::SurfaceId());
  }
  if (HasPrimarySurface()) {
    client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
        viz::SurfaceId(), current_frame_size_in_dip_, GetGutterColor(),
        cc::DeadlinePolicy::UseDefaultDeadline(), false);
  }

  if (!HasSavedFrame())
    return;
  std::vector<viz::SurfaceId> surface_ids = {GetCurrentSurfaceId()};
  DCHECK(host_frame_sink_manager_);
  host_frame_sink_manager_->EvictSurfaces(surface_ids);
  frame_evictor_->DiscardedFrame();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::CompositorObserver implementation:

void DelegatedFrameHost::OnCompositingDidCommit(ui::Compositor* compositor) {
}

void DelegatedFrameHost::OnCompositingStarted(ui::Compositor* compositor,
                                              base::TimeTicks start_time) {}

void DelegatedFrameHost::OnCompositingEnded(ui::Compositor* compositor) {}

void DelegatedFrameHost::OnCompositingChildResizing(
    ui::Compositor* compositor) {}

void DelegatedFrameHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor, compositor_);
  DetachFromCompositor();
  DCHECK(!compositor_);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ContextFactoryObserver implementation:

void DelegatedFrameHost::OnLostSharedContext() {}

void DelegatedFrameHost::OnLostVizProcess() {
  // With OOP-D renderer surface was destroyed if the GPU process crashed. Reset
  // the fallback Surface but leave the primary so we embed the renderer surface
  // again.
  if (HasFallbackSurface()) {
    client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
        viz::SurfaceId());
  }

  if (HasSavedFrame())
    frame_evictor_->DiscardedFrame();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

void DelegatedFrameHost::AttachToCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
  if (!compositor)
    return;
  compositor_ = compositor;
  compositor_->AddObserver(this);
  if (should_register_frame_sink_id_)
    compositor_->AddChildFrameSink(frame_sink_id_);
}

void DelegatedFrameHost::DetachFromCompositor() {
  if (!compositor_)
    return;
  if (compositor_->HasObserver(this))
    compositor_->RemoveObserver(this);
  if (should_register_frame_sink_id_)
    compositor_->RemoveChildFrameSink(frame_sink_id_);
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
  DCHECK(host_frame_sink_manager_);
  support_ = host_frame_sink_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, needs_sync_points);
  if (compositor_ && should_register_frame_sink_id_)
    compositor_->AddChildFrameSink(frame_sink_id_);
  if (needs_begin_frame_)
    support_->SetNeedsBeginFrame(true);
}

void DelegatedFrameHost::ResetCompositorFrameSinkSupport() {
  if (!support_)
    return;
  if (compositor_ && should_register_frame_sink_id_)
    compositor_->RemoveChildFrameSink(frame_sink_id_);
  support_.reset();
}

void DelegatedFrameHost::DidNavigate() {
  first_local_surface_id_after_navigation_ = pending_local_surface_id_;
  received_frame_after_navigation_ = false;
}

bool DelegatedFrameHost::IsPrimarySurfaceEvicted() const {
  return active_local_surface_id_ == pending_local_surface_id_ &&
         !HasSavedFrame();
}

void DelegatedFrameHost::WindowTitleChanged(const std::string& title) {
  if (host_frame_sink_manager_)
    host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_, title);
}

void DelegatedFrameHost::TakeFallbackContentFrom(DelegatedFrameHost* other) {
  if (!other->HasFallbackSurface() || HasFallbackSurface())
    return;

  if (!HasPrimarySurface()) {
    client_->DelegatedFrameHostGetLayer()->SetShowPrimarySurface(
        *other->client_->DelegatedFrameHostGetLayer()->GetFallbackSurfaceId(),
        other->client_->DelegatedFrameHostGetLayer()->size(),
        other->client_->DelegatedFrameHostGetLayer()->background_color(),
        cc::DeadlinePolicy::UseDefaultDeadline(),
        false /* stretch_content_to_fill_bounds */);
  }

  client_->DelegatedFrameHostGetLayer()->SetFallbackSurfaceId(
      *other->client_->DelegatedFrameHostGetLayer()->GetFallbackSurfaceId());
}

}  // namespace content
