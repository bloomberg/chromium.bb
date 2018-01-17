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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "media/base/video_frame.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/layout.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

namespace {

// Set when no browser compositors should remain alive.
bool g_has_shut_down = false;

// The number of placeholder objects allocated. If this reaches zero, then
// the RecyclableCompositorMac being held on to for recycling,
// |g_spare_recyclable_compositors|, will be freed.
uint32_t g_browser_compositor_count = 0;

// A spare RecyclableCompositorMac kept around for recycling.
base::LazyInstance<base::circular_deque<
    std::unique_ptr<RecyclableCompositorMac>>>::DestructorAtExit
    g_spare_recyclable_compositors;

void ReleaseSpareCompositors() {
  // Allow at most one spare recyclable compositor.
  while (g_spare_recyclable_compositors.Get().size() > 1)
    g_spare_recyclable_compositors.Get().pop_front();

  if (!g_browser_compositor_count)
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

  // Indicate that the recyclable compositor should be destroyed, and no future
  // compositors should be recycled.
  static void DisableRecyclingForShutdown();

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
  DCHECK(compositor);
  content::ImageTransportFactory::GetInstance()
      ->SetCompositorSuspendedForRecycle(compositor->compositor(), true);

  // It is an error to have a browser compositor continue to exist after
  // shutdown.
  CHECK(!g_has_shut_down);

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
    const viz::FrameSinkId& frame_sink_id)
    : client_(client),
      accelerated_widget_mac_ns_view_(accelerated_widget_mac_ns_view),
      enable_viz_(
          base::FeatureList::IsEnabled(features::kVizDisplayCompositor)),
      weak_factory_(this) {
  g_browser_compositor_count += 1;

  root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
  delegated_frame_host_.reset(new DelegatedFrameHost(
      frame_sink_id, this, features::IsSurfaceSynchronizationEnabled(),
      enable_viz_));

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

  DCHECK_GT(g_browser_compositor_count, 0u);
  g_browser_compositor_count -= 1;

  // If there are no compositors allocated, destroy the recyclable
  // RecyclableCompositorMac.
  if (!g_browser_compositor_count)
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

void BrowserCompositorMac::CopyCompleted(
    base::WeakPtr<BrowserCompositorMac> browser_compositor,
    const ReadbackRequestCallback& callback,
    const SkBitmap& bitmap,
    ReadbackResponse response) {
  callback.Run(bitmap, response);
  if (browser_compositor) {
    browser_compositor->outstanding_copy_count_ -= 1;
    browser_compositor->UpdateState();
  }
}

void BrowserCompositorMac::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const ReadbackRequestCallback& callback,
    SkColorType preferred_color_type) {
  DCHECK(delegated_frame_host_);
  DCHECK(state_ == HasAttachedCompositor);
  outstanding_copy_count_ += 1;

  auto callback_with_decrement =
      base::Bind(&BrowserCompositorMac::CopyCompleted,
                 weak_factory_.GetWeakPtr(), callback);
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, dst_size, callback_with_decrement, preferred_color_type);
}

void BrowserCompositorMac::CopyToVideoFrameCompleted(
    base::WeakPtr<BrowserCompositorMac> browser_compositor,
    const base::Callback<void(const gfx::Rect&, bool)>& callback,
    const gfx::Rect& rect,
    bool result) {
  callback.Run(rect, result);
  if (browser_compositor) {
    browser_compositor->outstanding_copy_count_ -= 1;
    browser_compositor->UpdateState();
  }
}

void BrowserCompositorMac::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    scoped_refptr<media::VideoFrame> target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  DCHECK(delegated_frame_host_);
  DCHECK(state_ == HasAttachedCompositor);
  outstanding_copy_count_ += 1;

  auto callback_with_decrement =
      base::Bind(&BrowserCompositorMac::CopyToVideoFrameCompleted,
                 weak_factory_.GetWeakPtr(), callback);

  delegated_frame_host_->CopyFromCompositingSurfaceToVideoFrame(
      src_subrect, std::move(target), callback_with_decrement);
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

void BrowserCompositorMac::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame) {
  // Compute the frame size based on the root render pass rect size.
  viz::RenderPass* root_pass = frame.render_pass_list.back().get();
  float scale_factor = frame.metadata.device_scale_factor;
  gfx::Size pixel_size = root_pass->output_rect.size();
  gfx::Size dip_size = gfx::ConvertSizeToDIP(scale_factor, pixel_size);
  root_layer_->SetBounds(gfx::Rect(dip_size));
  if (recyclable_compositor_) {
    recyclable_compositor_->compositor()->SetScaleAndSize(
        scale_factor, pixel_size, compositor_surface_id_);
  }
  delegated_frame_host_->SubmitCompositorFrame(local_surface_id,
                                               std::move(frame), nullptr);
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

void BrowserCompositorMac::SetDisplayColorSpace(
    const gfx::ColorSpace& color_space) {
  if (recyclable_compositor_)
    recyclable_compositor_->compositor()->SetDisplayColorSpace(color_space);
}

void BrowserCompositorMac::WasResized() {
  // In non-viz, the ui::Compositor is resized in sync with frames coming from
  // the renderer. In viz, the ui::Compositor can only resize in sync with the
  // NSView.
  if (!enable_viz_) {
    GetDelegatedFrameHost()->WasResized();
    return;
  }

  if (!recyclable_compositor_)
    return;

  gfx::Size dip_size;
  float scale_factor = 1.f;
  GetViewProperties(&dip_size, &scale_factor, nullptr);
  gfx::Size pixel_size = gfx::ConvertSizeToPixel(scale_factor, dip_size);

  gfx::Size old_pixel_size = recyclable_compositor_->compositor()->size();
  float old_scale_factor =
      recyclable_compositor_->compositor()->device_scale_factor();

  if (pixel_size == old_pixel_size && scale_factor == old_scale_factor)
    return;

  delegated_frame_host_surface_id_ =
      parent_local_surface_id_allocator_.GenerateId();
  compositor_surface_id_ = parent_local_surface_id_allocator_.GenerateId();

  root_layer_->SetBounds(gfx::Rect(dip_size));
  recyclable_compositor_->compositor()->SetScaleAndSize(
      scale_factor, pixel_size, compositor_surface_id_);

  GetDelegatedFrameHost()->WasResized();
}

bool BrowserCompositorMac::HasFrameOfSize(const gfx::Size& desired_size) {
  if (recyclable_compositor_) {
    return recyclable_compositor_->accelerated_widget_mac()->HasFrameOfSize(
        desired_size);
  }
  return false;
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
  if (!render_widget_host_is_hidden_ || outstanding_copy_count_ > 0)
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
    recyclable_compositor_->accelerated_widget_mac()->SetNSView(
        accelerated_widget_mac_ns_view_);
    state_ = HasDetachedCompositor;
  }

  // Transition HasDetachedCompositor -> HasAttachedCompositor.
  if (state_ == HasDetachedCompositor && new_state == HasAttachedCompositor) {
    gfx::Size dip_size;
    float scale_factor = 1.f;
    gfx::ColorSpace color_space;
    GetViewProperties(&dip_size, &scale_factor, &color_space);
    gfx::Size pixel_size = gfx::ConvertSizeToPixel(scale_factor, dip_size);

    delegated_frame_host_->SetCompositor(recyclable_compositor_->compositor());
    delegated_frame_host_->WasShown(ui::LatencyInfo());
    // Unsuspend the browser compositor after showing the delegated frame host.
    // If there is not a saved delegated frame, then the delegated frame host
    // will keep the compositor locked until a delegated frame is swapped.
    recyclable_compositor_->compositor()->SetDisplayColorSpace(color_space);
    recyclable_compositor_->compositor()->SetScaleAndSize(
        scale_factor, pixel_size, compositor_surface_id_);
    recyclable_compositor_->Unsuspend();
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
    recyclable_compositor_->compositor()->SetScaleAndSize(
        1.0, gfx::Size(0, 0), viz::LocalSurfaceId());
    recyclable_compositor_->compositor()->SetRootLayer(nullptr);
    RecyclableCompositorMac::Recycle(std::move(recyclable_compositor_));
    state_ = HasNoCompositor;
  }
}

void BrowserCompositorMac::GetViewProperties(
    gfx::Size* bounds_in_dip,
    float* scale_factor,
    gfx::ColorSpace* color_space) const {
  NSView* ns_view =
      accelerated_widget_mac_ns_view_->AcceleratedWidgetGetNSView();
  if (bounds_in_dip) {
    NSSize dip_ns_size = [ns_view bounds].size;
    *bounds_in_dip = gfx::Size(dip_ns_size.width, dip_ns_size.height);
  }
  if (scale_factor || color_space) {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestView(ns_view);
    if (scale_factor)
      *scale_factor = display.device_scale_factor();
    if (color_space)
      *color_space = display.color_space();
  }
}

// static
void BrowserCompositorMac::DisableRecyclingForShutdown() {
  g_has_shut_down = true;
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
  return state_ == HasAttachedCompositor;
}

SkColor BrowserCompositorMac::DelegatedFrameHostGetGutterColor() const {
  return client_->BrowserCompositorMacGetGutterColor();
}

gfx::Size BrowserCompositorMac::DelegatedFrameHostDesiredSizeInDIP() const {
  gfx::Size dip_size;
  GetViewProperties(&dip_size, nullptr, nullptr);
  if (enable_viz_) {
    if (recyclable_compositor_) {
      const gfx::Size& pixel_size =
          recyclable_compositor_->compositor()->size();
      float scale_factor =
          recyclable_compositor_->compositor()->device_scale_factor();
      dip_size = gfx::ConvertSizeToDIP(scale_factor, pixel_size);
    }
  }
  return dip_size;
}

bool BrowserCompositorMac::DelegatedFrameCanCreateResizeLock() const {
  // Mac uses the RenderWidgetResizeHelper instead of a resize lock.
  return false;
}

viz::LocalSurfaceId BrowserCompositorMac::GetLocalSurfaceId() const {
  return delegated_frame_host_surface_id_;
}

std::unique_ptr<CompositorResizeLock>
BrowserCompositorMac::DelegatedFrameHostCreateResizeLock() {
  NOTREACHED();
  return nullptr;
}

void BrowserCompositorMac::OnBeginFrame(base::TimeTicks frame_time) {
  client_->BrowserCompositorMacOnBeginFrame();
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

}  // namespace content
