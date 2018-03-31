// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browser_compositor_view_mac.h"

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/compositor_resize_lock.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/layout.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

namespace {

// Weak pointers to all BrowserCompositorMac instances, used to
// - Determine if a spare RecyclableCompositorMac should be kept around (one
//   should be only if there exists at least one BrowserCompositorMac).
// - Force all ui::Compositors to be destroyed at shut-down (because the NSView
//   signals to shut down will come in very late, long after things that the
//   ui::Compositor depend on have been destroyed).
//   https://crbug.com/805726
base::LazyInstance<std::set<BrowserCompositorMac*>>::Leaky
    g_browser_compositors;

// A spare RecyclableCompositorMac kept around for recycling.
base::LazyInstance<base::circular_deque<
    std::unique_ptr<RecyclableCompositorMac>>>::DestructorAtExit
    g_spare_recyclable_compositors;

void ReleaseSpareCompositors() {
  // Allow at most one spare recyclable compositor.
  while (g_spare_recyclable_compositors.Get().size() > 1)
    g_spare_recyclable_compositors.Get().pop_front();

  if (g_browser_compositors.Get().empty())
    g_spare_recyclable_compositors.Get().clear();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RecyclableCompositorMac

// A ui::Compositor and a gfx::AcceleratedWidget (and helper) that it draws
// into. This structure is used to efficiently recycle these structures across
// tabs (because creating a new ui::Compositor for each tab would be expensive
// in terms of time and resources).
class RecyclableCompositorMac : public ui::CompositorObserver {
 public:
  ~RecyclableCompositorMac() override;

  // Create a compositor, or recycle a preexisting one.
  static std::unique_ptr<RecyclableCompositorMac> Create();

  // Delete a compositor, or allow it to be recycled.
  static void Recycle(std::unique_ptr<RecyclableCompositorMac> compositor);

  ui::Compositor* compositor() { return &compositor_; }
  ui::AcceleratedWidgetMac* accelerated_widget_mac() {
    return accelerated_widget_mac_.get();
  }

  // Suspend will prevent the compositor from producing new frames. This should
  // be called to avoid creating spurious frames while changing state.
  // Compositors are created as suspended.
  void Suspend();
  void Unsuspend();

 private:
  RecyclableCompositorMac();

  // ui::CompositorObserver implementation:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override {}
  void OnCompositingEnded(ui::Compositor* compositor) override {}
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override {}
  void OnCompositingChildResizing(ui::Compositor* compositor) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}

  std::unique_ptr<ui::AcceleratedWidgetMac> accelerated_widget_mac_;
  ui::Compositor compositor_;
  std::unique_ptr<ui::CompositorLock> compositor_suspended_lock_;

  DISALLOW_COPY_AND_ASSIGN(RecyclableCompositorMac);
};

RecyclableCompositorMac::RecyclableCompositorMac()
    : accelerated_widget_mac_(new ui::AcceleratedWidgetMac()),
      compositor_(content::GetContextFactoryPrivate()->AllocateFrameSinkId(),
                  content::GetContextFactory(),
                  content::GetContextFactoryPrivate(),
                  ui::WindowResizeHelperMac::Get()->task_runner(),
                  features::IsSurfaceSynchronizationEnabled(),
                  false /* enable_pixel_canvas */) {
  compositor_.SetAcceleratedWidget(
      accelerated_widget_mac_->accelerated_widget());
  Suspend();
  compositor_.AddObserver(this);
}

RecyclableCompositorMac::~RecyclableCompositorMac() {
  compositor_.RemoveObserver(this);
}

void RecyclableCompositorMac::Suspend() {
  // Requests a compositor lock without a timeout.
  compositor_suspended_lock_ =
      compositor_.GetCompositorLock(nullptr, base::TimeDelta());
}

void RecyclableCompositorMac::Unsuspend() {
  compositor_suspended_lock_ = nullptr;
}

void RecyclableCompositorMac::OnCompositingDidCommit(
    ui::Compositor* compositor_that_did_commit) {
  DCHECK_EQ(compositor_that_did_commit, compositor());
  content::ImageTransportFactory::GetInstance()
      ->SetCompositorSuspendedForRecycle(compositor(), false);
}

// static
std::unique_ptr<RecyclableCompositorMac> RecyclableCompositorMac::Create() {
  DCHECK(ui::WindowResizeHelperMac::Get()->task_runner());
  if (!g_spare_recyclable_compositors.Get().empty()) {
    std::unique_ptr<RecyclableCompositorMac> result;
    result = std::move(g_spare_recyclable_compositors.Get().front());
    g_spare_recyclable_compositors.Get().pop_front();
    return result;
  }
  return std::unique_ptr<RecyclableCompositorMac>(new RecyclableCompositorMac);
}

// static
void RecyclableCompositorMac::Recycle(
    std::unique_ptr<RecyclableCompositorMac> compositor) {
  content::ImageTransportFactory::GetInstance()
      ->SetCompositorSuspendedForRecycle(compositor->compositor(), true);

  // Make this RecyclableCompositorMac recyclable for future instances.
  g_spare_recyclable_compositors.Get().push_back(std::move(compositor));

  // Post a task to free up the spare ui::Compositors when needed. Post this
  // to the browser main thread so that we won't free any compositors while
  // in a nested loop waiting to put up a new frame.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(&ReleaseSpareCompositors));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorMac

BrowserCompositorMac::BrowserCompositorMac(
    ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view,
    BrowserCompositorMacClient* client,
    bool render_widget_host_is_hidden,
    bool ns_view_attached_to_window,
    const display::Display& initial_display,
    const viz::FrameSinkId& frame_sink_id)
    : client_(client),
      accelerated_widget_mac_ns_view_(accelerated_widget_mac_ns_view),
      dfh_display_(initial_display),
      weak_factory_(this) {
  g_browser_compositors.Get().insert(this);

  root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
  delegated_frame_host_.reset(new DelegatedFrameHost(
      frame_sink_id, this, features::IsSurfaceSynchronizationEnabled(),
      base::FeatureList::IsEnabled(features::kVizDisplayCompositor),
      true /* should_register_frame_sink_id */));

  SetRenderWidgetHostIsHidden(render_widget_host_is_hidden);
  SetNSViewAttachedToWindow(ns_view_attached_to_window);
}

BrowserCompositorMac::~BrowserCompositorMac() {
  // Ensure that copy callbacks completed or cancelled during further tear-down
  // do not call back into this.
  weak_factory_.InvalidateWeakPtrs();

  TransitionToState(HasNoCompositor);
  delegated_frame_host_.reset();
  root_layer_.reset();

  size_t num_erased = g_browser_compositors.Get().erase(this);
  DCHECK_EQ(1u, num_erased);

  // If there are no compositors allocated, destroy the recyclable
  // RecyclableCompositorMac.
  if (g_browser_compositors.Get().empty())
    g_spare_recyclable_compositors.Get().clear();
}

gfx::AcceleratedWidget BrowserCompositorMac::GetAcceleratedWidget() {
  if (recyclable_compositor_) {
    return recyclable_compositor_->accelerated_widget_mac()
        ->accelerated_widget();
  }
  return gfx::kNullAcceleratedWidget;
}

DelegatedFrameHost* BrowserCompositorMac::GetDelegatedFrameHost() {
  DCHECK(delegated_frame_host_);
  return delegated_frame_host_.get();
}

void BrowserCompositorMac::ClearCompositorFrame() {
  // Make sure that we no longer hold a compositor lock by un-suspending the
  // compositor. This ensures that we are able to swap in a new blank frame to
  // replace any old content.
  // https://crbug.com/739621
  if (recyclable_compositor_)
    recyclable_compositor_->Unsuspend();
  if (delegated_frame_host_)
    delegated_frame_host_->ClearDelegatedFrame();
}

viz::FrameSinkId BrowserCompositorMac::GetRootFrameSinkId() {
  if (recyclable_compositor_->compositor())
    return recyclable_compositor_->compositor()->frame_sink_id();
  return viz::FrameSinkId();
}

void BrowserCompositorMac::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  renderer_compositor_frame_sink_ = renderer_compositor_frame_sink;
  delegated_frame_host_->DidCreateNewRendererCompositorFrameSink(
      renderer_compositor_frame_sink_);
}

void BrowserCompositorMac::OnDidNotProduceFrame(const viz::BeginFrameAck& ack) {
  delegated_frame_host_->DidNotProduceFrame(ack);
}

void BrowserCompositorMac::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
  if (recyclable_compositor_) {
    recyclable_compositor_->compositor()->SetBackgroundColor(background_color_);
  }
}

bool BrowserCompositorMac::UpdateNSViewAndDisplay(
    const gfx::Size& new_size_dip,
    const display::Display& new_display) {
  if (new_size_dip == dfh_size_dip_ && new_display == dfh_display_)
    return false;

  bool needs_new_surface_id =
      new_size_dip != dfh_size_dip_ ||
      new_display.device_scale_factor() != dfh_display_.device_scale_factor();

  dfh_display_ = new_display;
  dfh_size_dip_ = new_size_dip;
  dfh_size_pixels_ = gfx::ConvertSizeToPixel(dfh_display_.device_scale_factor(),
                                             dfh_size_dip_);

  if (needs_new_surface_id) {
    if (recyclable_compositor_)
      recyclable_compositor_->Suspend();
    GetDelegatedFrameHost()->WasResized(
        dfh_local_surface_id_allocator_.GenerateId(), dfh_size_dip_,
        cc::DeadlinePolicy::UseExistingDeadline());
  }

  if (recyclable_compositor_) {
    recyclable_compositor_->compositor()->SetDisplayColorSpace(
        dfh_display_.color_space());
  }
  return true;
}

void BrowserCompositorMac::UpdateForAutoResize(const gfx::Size& new_size_dip) {
  if (new_size_dip == dfh_size_dip_)
    return;

  dfh_size_dip_ = new_size_dip;
  dfh_size_pixels_ = gfx::ConvertSizeToPixel(dfh_display_.device_scale_factor(),
                                             dfh_size_dip_);

  if (recyclable_compositor_)
    recyclable_compositor_->Suspend();
  GetDelegatedFrameHost()->WasResized(
      dfh_local_surface_id_allocator_.GenerateId(), dfh_size_dip_,
      cc::DeadlinePolicy::UseExistingDeadline());
}

void BrowserCompositorMac::UpdateVSyncParameters(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  if (recyclable_compositor_) {
    recyclable_compositor_->compositor()->SetDisplayVSyncParameters(
        timebase, interval);
  }
}

void BrowserCompositorMac::SetRenderWidgetHostIsHidden(bool hidden) {
  render_widget_host_is_hidden_ = hidden;
  UpdateState();
}

void BrowserCompositorMac::SetNSViewAttachedToWindow(bool attached) {
  ns_view_attached_to_window_ = attached;
  UpdateState();
}

void BrowserCompositorMac::UpdateState() {
  if (!render_widget_host_is_hidden_)
    TransitionToState(HasAttachedCompositor);
  else if (ns_view_attached_to_window_)
    TransitionToState(HasDetachedCompositor);
  else
    TransitionToState(HasNoCompositor);
}

void BrowserCompositorMac::TransitionToState(State new_state) {
  // Transition HasNoCompositor -> HasDetachedCompositor.
  if (state_ == HasNoCompositor && new_state != HasNoCompositor) {
    recyclable_compositor_ = RecyclableCompositorMac::Create();
    recyclable_compositor_->compositor()->SetRootLayer(root_layer_.get());
    recyclable_compositor_->compositor()->SetBackgroundColor(background_color_);
    recyclable_compositor_->compositor()->SetDisplayColorSpace(
        dfh_display_.color_space());
    recyclable_compositor_->accelerated_widget_mac()->SetNSView(
        accelerated_widget_mac_ns_view_);
    state_ = HasDetachedCompositor;
  }

  // Transition HasDetachedCompositor -> HasAttachedCompositor.
  if (state_ == HasDetachedCompositor && new_state == HasAttachedCompositor) {
    delegated_frame_host_->SetCompositor(recyclable_compositor_->compositor());
    delegated_frame_host_->WasShown(GetRendererLocalSurfaceId(), dfh_size_dip_,
                                    ui::LatencyInfo());

    // If there exists a saved frame ready to display, unsuspend the compositor
    // now (if one is not ready, the compositor will unsuspend on first surface
    // activation).
    if (delegated_frame_host_->HasSavedFrame()) {
      if (compositor_scale_factor_ != dfh_display_.device_scale_factor() ||
          compositor_size_pixels_ != dfh_size_pixels_) {
        compositor_scale_factor_ = dfh_display_.device_scale_factor();
        compositor_size_pixels_ = dfh_size_pixels_;
        root_layer_->SetBounds(gfx::Rect(gfx::ConvertSizeToDIP(
            compositor_scale_factor_, compositor_size_pixels_)));
        recyclable_compositor_->compositor()->SetScaleAndSize(
            compositor_scale_factor_, compositor_size_pixels_,
            compositor_local_surface_id_allocator_.GenerateId());
      }
      recyclable_compositor_->Unsuspend();
    }

    state_ = HasAttachedCompositor;
  }

  // Transition HasAttachedCompositor -> HasDetachedCompositor.
  if (state_ == HasAttachedCompositor && new_state != HasAttachedCompositor) {
    // Ensure that any changes made to the ui::Compositor do not result in new
    // frames being produced.
    recyclable_compositor_->Suspend();
    // Marking the DelegatedFrameHost as removed from the window hierarchy is
    // necessary to remove all connections to its old ui::Compositor.
    delegated_frame_host_->WasHidden();
    delegated_frame_host_->ResetCompositor();
    state_ = HasDetachedCompositor;
  }

  // Transition HasDetachedCompositor -> HasNoCompositor.
  if (state_ == HasDetachedCompositor && new_state == HasNoCompositor) {
    recyclable_compositor_->accelerated_widget_mac()->ResetNSView();
    compositor_scale_factor_ = 1.f;
    compositor_size_pixels_ = gfx::Size();
    compositor_local_surface_id_allocator_.Invalidate();
    recyclable_compositor_->compositor()->SetScaleAndSize(
        compositor_scale_factor_, compositor_size_pixels_,
        compositor_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
    recyclable_compositor_->compositor()->SetRootLayer(nullptr);
    RecyclableCompositorMac::Recycle(std::move(recyclable_compositor_));
    state_ = HasNoCompositor;
  }
}

// static
void BrowserCompositorMac::DisableRecyclingForShutdown() {
  // Ensure that the client has destroyed its BrowserCompositorViewMac before
  // it dependencies are destroyed.
  // https://crbug.com/805726
  while (!g_browser_compositors.Get().empty()) {
    BrowserCompositorMac* browser_compositor =
        *g_browser_compositors.Get().begin();
    browser_compositor->client_->DestroyCompositorForShutdown();
  }
  g_spare_recyclable_compositors.Get().clear();
}

void BrowserCompositorMac::SetNeedsBeginFrames(bool needs_begin_frames) {
  delegated_frame_host_->SetNeedsBeginFrames(needs_begin_frames);
}

void BrowserCompositorMac::SetWantsAnimateOnlyBeginFrames() {
  delegated_frame_host_->SetWantsAnimateOnlyBeginFrames();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, public:

ui::Layer* BrowserCompositorMac::DelegatedFrameHostGetLayer() const {
  return root_layer_.get();
}

bool BrowserCompositorMac::DelegatedFrameHostIsVisible() const {
  return state_ == HasAttachedCompositor || state_ == HasDetachedCompositor;
}

SkColor BrowserCompositorMac::DelegatedFrameHostGetGutterColor() const {
  return client_->BrowserCompositorMacGetGutterColor();
}

bool BrowserCompositorMac::DelegatedFrameCanCreateResizeLock() const {
  // Mac uses the RenderWidgetResizeHelper instead of a resize lock.
  return false;
}

std::unique_ptr<CompositorResizeLock>
BrowserCompositorMac::DelegatedFrameHostCreateResizeLock() {
  NOTREACHED();
  return nullptr;
}

void BrowserCompositorMac::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  if (!recyclable_compositor_)
    return;

  recyclable_compositor_->Unsuspend();

  // Resize the compositor to match the current frame size, if needed.
  if (compositor_size_pixels_ == surface_info.size_in_pixels() &&
      compositor_scale_factor_ == surface_info.device_scale_factor()) {
    return;
  }
  compositor_size_pixels_ = surface_info.size_in_pixels();
  compositor_scale_factor_ = surface_info.device_scale_factor();
  root_layer_->SetBounds(gfx::Rect(gfx::ConvertSizeToDIP(
      compositor_scale_factor_, compositor_size_pixels_)));
  recyclable_compositor_->compositor()->SetScaleAndSize(
      compositor_scale_factor_, compositor_size_pixels_,
      compositor_local_surface_id_allocator_.GenerateId());

  // Disable screen updates until the frame of the new size appears (because the
  // content is drawn in the GPU process, it may change before we want it to).
  if (repaint_state_ == RepaintState::Paused) {
    bool compositor_is_nsview_size =
        compositor_size_pixels_ == dfh_size_pixels_ &&
        compositor_scale_factor_ == dfh_display_.device_scale_factor();
    if (compositor_is_nsview_size || repaint_auto_resize_enabled_) {
      NSDisableScreenUpdates();
      repaint_state_ = RepaintState::ScreenUpdatesDisabled;
    }
  }
}

void BrowserCompositorMac::OnBeginFrame(base::TimeTicks frame_time) {
  client_->BrowserCompositorMacOnBeginFrame(frame_time);
}

bool BrowserCompositorMac::IsAutoResizeEnabled() const {
  NOTREACHED();
  return false;
}

void BrowserCompositorMac::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

ui::Compositor* BrowserCompositorMac::CompositorForTesting() const {
  if (recyclable_compositor_)
    return recyclable_compositor_->compositor();
  return nullptr;
}

void BrowserCompositorMac::DidNavigate() {
  const viz::LocalSurfaceId& new_local_surface_id =
      dfh_local_surface_id_allocator_.GenerateId();
  client_->WasResized();
  delegated_frame_host_->WasResized(new_local_surface_id, dfh_size_dip_,
                                    cc::DeadlinePolicy::UseExistingDeadline());
  delegated_frame_host_->DidNavigate();
}

void BrowserCompositorMac::DidReceiveFirstFrameAfterNavigation() {
  client_->DidReceiveFirstFrameAfterNavigation();
}

void BrowserCompositorMac::BeginPauseForFrame(bool auto_resize_enabled) {
  repaint_auto_resize_enabled_ = auto_resize_enabled;
  repaint_state_ = RepaintState::Paused;
}

void BrowserCompositorMac::EndPauseForFrame() {
  if (repaint_state_ == RepaintState::ScreenUpdatesDisabled)
    NSEnableScreenUpdates();
  repaint_state_ = RepaintState::None;
}

bool BrowserCompositorMac::ShouldContinueToPauseForFrame() const {
  // The renderer won't produce a frame if its frame sink hasn't been created
  // yet.
  if (!renderer_compositor_frame_sink_)
    return false;

  if (!recyclable_compositor_)
    return false;

  return !recyclable_compositor_->accelerated_widget_mac()->HasFrameOfSize(
      dfh_size_dip_);
}

void BrowserCompositorMac::GetRendererScreenInfo(
    ScreenInfo* screen_info) const {
  DisplayUtil::DisplayToScreenInfo(screen_info, dfh_display_);
}

const viz::LocalSurfaceId& BrowserCompositorMac::GetRendererLocalSurfaceId()
    const {
  return dfh_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

}  // namespace content
