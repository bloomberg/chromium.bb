// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/render_process_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "services/service_manager/runner/common/client_util.h"
#include "third_party/WebKit/public/platform/WebTouchEvent.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

// static
RenderWidgetHostViewChildFrame* RenderWidgetHostViewChildFrame::Create(
    RenderWidgetHost* widget) {
  RenderWidgetHostViewChildFrame* view =
      new RenderWidgetHostViewChildFrame(widget);
  view->Init();
  return view;
}

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      frame_sink_id_(
          base::checked_cast<uint32_t>(widget_host->GetProcess()->GetID()),
          base::checked_cast<uint32_t>(widget_host->GetRoutingID())),
      next_surface_sequence_(1u),
      current_surface_scale_factor_(1.f),
      frame_connector_(nullptr),
      background_color_(SK_ColorWHITE),
      weak_factory_(this) {
  if (!service_manager::ServiceManagerIsRemote()) {
    GetFrameSinkManager()->surface_manager()->RegisterFrameSinkId(
        frame_sink_id_);
    CreateCompositorFrameSinkSupport();
  }
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
  if (!service_manager::ServiceManagerIsRemote()) {
    ResetCompositorFrameSinkSupport();
    if (GetFrameSinkManager()) {
      GetFrameSinkManager()->surface_manager()->InvalidateFrameSinkId(
          frame_sink_id_);
    }
  }
}

void RenderWidgetHostViewChildFrame::Init() {
  RegisterFrameSinkId();
  host_->SetView(this);
  GetTextInputManager();
}

void RenderWidgetHostViewChildFrame::
    DetachFromTouchSelectionClientManagerIfNecessary() {
  if (!selection_controller_client_)
    return;

  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (root_view) {
    auto* manager = root_view->GetTouchSelectionControllerClientManager();
    if (manager)
      manager->RemoveObserver(this);
  }

  selection_controller_client_.reset();
}

void RenderWidgetHostViewChildFrame::SetCrossProcessFrameConnector(
    CrossProcessFrameConnector* frame_connector) {
  if (frame_connector_ == frame_connector)
    return;

  if (frame_connector_) {
    if (parent_frame_sink_id_.is_valid() &&
        !service_manager::ServiceManagerIsRemote()) {
      GetFrameSinkManager()->UnregisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                          frame_sink_id_);
    }
    parent_frame_sink_id_ = viz::FrameSinkId();
    local_surface_id_ = viz::LocalSurfaceId();

    // Unlocks the mouse if this RenderWidgetHostView holds the lock.
    UnlockMouse();
    DetachFromTouchSelectionClientManagerIfNecessary();
  }
  frame_connector_ = frame_connector;
  if (frame_connector_) {
    RenderWidgetHostViewBase* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();
    if (parent_view) {
      parent_frame_sink_id_ = parent_view->GetFrameSinkId();
      DCHECK(parent_frame_sink_id_.is_valid());
      if (!service_manager::ServiceManagerIsRemote()) {
        GetFrameSinkManager()->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                          frame_sink_id_);
      }
    }

    auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
    if (root_view) {
      // Make sure we're not using the zero-valued default for
      // current_device_scale_factor_.
      current_device_scale_factor_ = root_view->current_device_scale_factor();
      if (current_device_scale_factor_ == 0.f)
        current_device_scale_factor_ = 1.f;

      auto* manager = root_view->GetTouchSelectionControllerClientManager();
      if (manager) {
        // We will only have a manager on Aura (and eventually Android).
        // TODO(wjmaclean): update this comment when TSE OOPIF support becomes
        // available on Android.
        selection_controller_client_ =
            base::MakeUnique<TouchSelectionControllerClientChildFrame>(this,
                                                                       manager);
        manager->AddObserver(this);
      }
    }
  }
}

void RenderWidgetHostViewChildFrame::OnManagerWillDestroy(
    TouchSelectionControllerClientManager* manager) {
  // We get the manager via the observer callback instead of through the
  // frame_connector_ since our connection to the root_view may disappear by
  // the time this function is called, but before frame_connector_ is reset.
  manager->RemoveObserver(this);
  selection_controller_client_.reset();
}

void RenderWidgetHostViewChildFrame::InitAsChild(
    gfx::NativeView parent_view) {
  NOTREACHED();
}

RenderWidgetHost* RenderWidgetHostViewChildFrame::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewChildFrame::SetSize(const gfx::Size& size) {
  host_->WasResized();
}

void RenderWidgetHostViewChildFrame::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());

  if (rect != last_screen_rect_) {
    last_screen_rect_ = rect;
    host_->SendScreenRects();
  }
}

void RenderWidgetHostViewChildFrame::Focus() {
}

bool RenderWidgetHostViewChildFrame::HasFocus() const {
  if (frame_connector_)
    return frame_connector_->HasFocus();
  return false;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() const {
  return has_frame_;
}

void RenderWidgetHostViewChildFrame::Show() {
  if (!host_->is_hidden())
    return;
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewChildFrame::Hide() {
  if (host_->is_hidden())
    return;
  host_->WasHidden();
}

bool RenderWidgetHostViewChildFrame::IsShowing() {
  return !host_->is_hidden();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() const {
  gfx::Rect rect;
  if (frame_connector_) {
    rect = frame_connector_->ChildFrameRect();

    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();

    // The parent_view can be null in tests when using a TestWebContents.
    if (parent_view) {
      // Translate frame_rect by the parent's RenderWidgetHostView offset.
      rect.Offset(parent_view->GetViewBounds().OffsetFromOrigin());
    }
  }
  return rect;
}

gfx::Size RenderWidgetHostViewChildFrame::GetVisibleViewportSize() const {
  // For subframes, the visual viewport corresponds to the main frame size, so
  // this bubbles up to the parent until it hits the main frame's
  // RenderWidgetHostView.
  //
  // Currently this excludes webview guests, since they expect the visual
  // viewport to return the guest's size rather than the page's; one reason why
  // is that Blink ends up using the visual viewport to calculate things like
  // window.innerWidth/innerHeight for main frames, and a guest is considered
  // to be a main frame.  This should be cleaned up eventually.
  bool is_guest = BrowserPluginGuest::IsGuest(RenderViewHostImpl::From(host_));
  if (frame_connector_ && !is_guest) {
    // An auto-resize set by the top-level frame overrides what would be
    // reported by embedding RenderWidgetHostViews.
    if (host_->delegate() && !host_->delegate()->GetAutoResizeSize().IsEmpty())
      return host_->delegate()->GetAutoResizeSize();

    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();
    // The parent_view can be null in unit tests when using a TestWebContents.
    if (parent_view)
      return parent_view->GetVisibleViewportSize();
  }
  return GetViewBounds().size();
}

gfx::Vector2dF RenderWidgetHostViewChildFrame::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() const {
  // TODO(ekaramad): To accomodate MimeHandlerViewGuest while embedded inside
  // OOPIF-webview, we need to return the native view to be used by
  // RenderWidgetHostViewGuest. Remove this once https://crbug.com/642826 is
  // fixed.
  if (frame_connector_)
    return frame_connector_->GetParentRenderWidgetHostView()->GetNativeView();
  return nullptr;
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED();
  return nullptr;
}

void RenderWidgetHostViewChildFrame::SetBackgroundColor(SkColor color) {
  background_color_ = color;

  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  host_->SetBackgroundOpaque(SkColorGetA(color) == SK_AlphaOPAQUE);
}

SkColor RenderWidgetHostViewChildFrame::background_color() const {
  return background_color_;
}

gfx::Size RenderWidgetHostViewChildFrame::GetPhysicalBackingSize() const {
  gfx::Size size;
  if (frame_connector_) {
    content::ScreenInfo screen_info;
    host_->GetScreenInfo(&screen_info);
    size = gfx::ScaleToCeiledSize(frame_connector_->ChildFrameRect().size(),
                                  screen_info.device_scale_factor);
  }
  return size;
}

void RenderWidgetHostViewChildFrame::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const WebCursor& cursor) {
  if (frame_connector_)
    frame_connector_->UpdateCursor(cursor);
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  // It is valid for an inner WebContents's SetIsLoading() to end up here.
  // This is because an inner WebContents's main frame's RenderWidgetHostView
  // is a RenderWidgetHostViewChildFrame. In contrast, when there is no
  // inner/outer WebContents, only subframe's RenderWidgetHostView can be a
  // RenderWidgetHostViewChildFrame which do not get a SetIsLoading() call.
  if (GuestMode::IsCrossProcessFrameGuest(
          WebContents::FromRenderViewHost(RenderViewHost::From(host_))))
    return;

  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  // FrameSinkIds registered with RenderWidgetHostInputEventRouter
  // have already been cleared when RenderWidgetHostViewBase notified its
  // observers of our impending destruction.
  if (frame_connector_) {
    frame_connector_->set_view(nullptr);
    SetCrossProcessFrameConnector(nullptr);
  }

  // We notify our observers about shutdown here since we are about to release
  // host_ and do not want any event calls coming from
  // RenderWidgetHostInputEventRouter afterwards.
  NotifyObserversAboutShutdown();

  host_->SetView(nullptr);
  host_ = nullptr;

  delete this;
}

void RenderWidgetHostViewChildFrame::SetTooltipText(
    const base::string16& tooltip_text) {
  frame_connector_->GetRootRenderWidgetHostView()->SetTooltipText(tooltip_text);
}

RenderWidgetHostViewBase* RenderWidgetHostViewChildFrame::GetParentView() {
  if (!frame_connector_)
    return nullptr;
  return frame_connector_->GetParentRenderWidgetHostView();
}

void RenderWidgetHostViewChildFrame::RegisterFrameSinkId() {
  // If Destroy() has been called before we get here, host_ may be null.
  if (host_ && host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    RenderWidgetHostInputEventRouter* router =
        host_->delegate()->GetInputEventRouter();
    if (!router->is_registered(frame_sink_id_))
      router->AddFrameSinkIdOwner(frame_sink_id_, this);
  }
}

void RenderWidgetHostViewChildFrame::UnregisterFrameSinkId() {
  DCHECK(host_);
  if (host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    host_->delegate()->GetInputEventRouter()->RemoveFrameSinkIdOwner(
        frame_sink_id_);
    DetachFromTouchSelectionClientManagerIfNecessary();
  }
}

void RenderWidgetHostViewChildFrame::UpdateViewportIntersection(
    const gfx::Rect& viewport_intersection) {
  if (host_)
    host_->Send(new ViewMsg_SetViewportIntersection(host_->GetRoutingID(),
                                                    viewport_intersection));
}

void RenderWidgetHostViewChildFrame::SetIsInert() {
  if (host_ && frame_connector_) {
    host_->Send(new ViewMsg_SetIsInert(host_->GetRoutingID(),
                                       frame_connector_->is_inert()));
  }
}

void RenderWidgetHostViewChildFrame::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  bool not_consumed = ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
                      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;

  if (!frame_connector_)
    return;
  if (wheel_scroll_latching_enabled()) {
    // GestureScrollBegin is a blocking event; It is forwarded for bubbling if
    // its ack is not consumed. For the rest of the scroll events
    // (GestureScrollUpdate, GestureScrollEnd, GestureFlingStart) the
    // frame_connector_ decides to forward them for bubbling if the
    // GestureScrollBegin event is forwarded.
    if ((event.GetType() == blink::WebInputEvent::kGestureScrollBegin &&
         not_consumed) ||
        event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
        event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
        event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
      frame_connector_->BubbleScrollEvent(event);
    }
  } else {
    // GestureScrollBegin is consumed by the target frame and not forwarded,
    // because we don't know whether we will need to bubble scroll until we
    // receive a GestureScrollUpdate ACK. GestureScrollUpdate with unused
    // scroll extent is forwarded for bubbling, while GestureScrollEnd is
    // always forwarded and handled according to current scroll state in the
    // RenderWidgetHostInputEventRouter.
    if ((event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
         not_consumed) ||
        event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
        event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
      frame_connector_->BubbleScrollEvent(event);
    }
  }
}

void RenderWidgetHostViewChildFrame::DidReceiveCompositorFrameAck(
    const std::vector<cc::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);
}

void RenderWidgetHostViewChildFrame::DidCreateNewRendererCompositorFrameSink(
    cc::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  ResetCompositorFrameSinkSupport();
  renderer_compositor_frame_sink_ = renderer_compositor_frame_sink;
  CreateCompositorFrameSinkSupport();
  has_frame_ = false;
}

void RenderWidgetHostViewChildFrame::ProcessCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  current_surface_size_ = frame.render_pass_list.back()->output_rect.size();
  current_surface_scale_factor_ = frame.metadata.device_scale_factor;

  bool result =
      support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  DCHECK(result);
  has_frame_ = true;

  if (local_surface_id_ != local_surface_id || HasEmbedderChanged()) {
    local_surface_id_ = local_surface_id;
    SendSurfaceInfoToEmbedder();
  }

  if (selection_controller_client_) {
    selection_controller_client_->UpdateSelectionBoundsIfNeeded(
        frame.metadata.selection, current_device_scale_factor_);
  }

  ProcessFrameSwappedCallbacks();
}

void RenderWidgetHostViewChildFrame::SendSurfaceInfoToEmbedder() {
  if (service_manager::ServiceManagerIsRemote())
    return;
  viz::SurfaceSequence sequence =
      viz::SurfaceSequence(frame_sink_id_, next_surface_sequence_++);
  cc::SurfaceManager* manager = GetFrameSinkManager()->surface_manager();
  viz::SurfaceId surface_id(frame_sink_id_, local_surface_id_);
  // The renderer process will satisfy this dependency when it creates a
  // SurfaceLayer.
  manager->RequireSequence(surface_id, sequence);
  viz::SurfaceInfo surface_info(surface_id, current_surface_scale_factor_,
                                current_surface_size_);
  SendSurfaceInfoToEmbedderImpl(surface_info, sequence);
}

void RenderWidgetHostViewChildFrame::SendSurfaceInfoToEmbedderImpl(
    const viz::SurfaceInfo& surface_info,
    const viz::SurfaceSequence& sequence) {
  frame_connector_->SetChildFrameSurface(surface_info, sequence);
}

void RenderWidgetHostViewChildFrame::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    cc::CompositorFrame frame) {
  TRACE_EVENT0("content",
               "RenderWidgetHostViewChildFrame::OnSwapCompositorFrame");
  last_scroll_offset_ = frame.metadata.root_scroll_offset;
  if (!frame_connector_)
    return;
  ProcessCompositorFrame(local_surface_id, std::move(frame));
}

void RenderWidgetHostViewChildFrame::OnDidNotProduceFrame(
    const cc::BeginFrameAck& ack) {
  support_->DidNotProduceFrame(ack);
}

void RenderWidgetHostViewChildFrame::OnSurfaceChanged(
    const viz::SurfaceInfo& surface_info) {
  viz::SurfaceSequence sequence(frame_sink_id_, next_surface_sequence_++);
  SendSurfaceInfoToEmbedderImpl(surface_info, sequence);
}

void RenderWidgetHostViewChildFrame::ProcessFrameSwappedCallbacks() {
  // We only use callbacks once, therefore we make a new list for registration
  // before we start, and discard the old list entries when we are done.
  FrameSwappedCallbackList process_callbacks;
  process_callbacks.swap(frame_swapped_callbacks_);
  for (std::unique_ptr<base::Closure>& callback : process_callbacks)
    callback->Run();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetBoundsInRootWindow() {
  gfx::Rect rect;
  if (frame_connector_) {
    RenderWidgetHostViewBase* root_view =
        frame_connector_->GetRootRenderWidgetHostView();

    // The root_view can be null in tests when using a TestWebContents.
    if (root_view)
      rect = root_view->GetBoundsInRootWindow();
  }
  return rect;
}

void RenderWidgetHostViewChildFrame::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
  if (!frame_connector_)
    return;

  frame_connector_->ForwardProcessAckedTouchEvent(touch, ack_result);
}

bool RenderWidgetHostViewChildFrame::LockMouse() {
  if (frame_connector_)
    return frame_connector_->LockMouse();
  return false;
}

void RenderWidgetHostViewChildFrame::UnlockMouse() {
  if (host_->delegate() && host_->delegate()->HasMouseLock(host_) &&
      frame_connector_)
    frame_connector_->UnlockMouse();
}

bool RenderWidgetHostViewChildFrame::IsMouseLocked() {
  if (!host_->delegate())
    return false;

  return host_->delegate()->HasMouseLock(host_);
}

viz::FrameSinkId RenderWidgetHostViewChildFrame::GetFrameSinkId() {
  return frame_sink_id_;
}

void RenderWidgetHostViewChildFrame::ProcessKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardKeyboardEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  if (event.delta_x != 0 || event.delta_y != 0 ||
      event.phase == blink::WebMouseWheelEvent::kPhaseEnded ||
      event.phase == blink::WebMouseWheelEvent::kPhaseCancelled ||
      event.momentum_phase == blink::WebMouseWheelEvent::kPhaseEnded ||
      event.momentum_phase == blink::WebMouseWheelEvent::kPhaseCancelled)
    host_->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  if (event.GetType() == blink::WebInputEvent::kTouchStart &&
      frame_connector_ && !frame_connector_->HasFocus()) {
    frame_connector_->FocusRootView();
  }

  host_->ForwardTouchEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardGestureEventWithLatencyInfo(event, latency);
}

gfx::Point RenderWidgetHostViewChildFrame::TransformPointToRootCoordSpace(
    const gfx::Point& point) {
  if (!frame_connector_ || !local_surface_id_.is_valid())
    return point;

  return frame_connector_->TransformPointToRootCoordSpace(
      point, viz::SurfaceId(frame_sink_id_, local_surface_id_));
}

bool RenderWidgetHostViewChildFrame::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    const viz::SurfaceId& original_surface,
    gfx::Point* transformed_point) {
  *transformed_point = point;
  if (!frame_connector_ || !local_surface_id_.is_valid())
    return false;

  return frame_connector_->TransformPointToLocalCoordSpace(
      point, original_surface,
      viz::SurfaceId(frame_sink_id_, local_surface_id_), transformed_point);
}

bool RenderWidgetHostViewChildFrame::TransformPointToCoordSpaceForView(
    const gfx::Point& point,
    RenderWidgetHostViewBase* target_view,
    gfx::Point* transformed_point) {
  if (!frame_connector_ || !local_surface_id_.is_valid())
    return false;

  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return frame_connector_->TransformPointToCoordSpaceForView(
      point, target_view, viz::SurfaceId(frame_sink_id_, local_surface_id_),
      transformed_point);
}

bool RenderWidgetHostViewChildFrame::IsRenderWidgetHostViewChildFrame() {
  return true;
}

void RenderWidgetHostViewChildFrame::WillSendScreenRects() {
  // TODO(kenrb): These represent post-initialization state updates that are
  // needed by the renderer. During normal OOPIF setup these are unnecessary,
  // as the parent renderer will send the information and it will be
  // immediately propagated to the OOPIF. However when an OOPIF navigates from
  // one process to another, the parent doesn't know that, and certain
  // browser-side state needs to be sent again. There is probably a less
  // spammy way to do this, but triggering on SendScreenRects() is reasonable
  // until somebody figures that out. RWHVCF::Init() is too early.
  if (frame_connector_) {
    UpdateViewportIntersection(frame_connector_->viewport_intersection());
    SetIsInert();
  }
}

#if defined(OS_MACOSX)
ui::AcceleratedWidgetMac*
RenderWidgetHostViewChildFrame::GetAcceleratedWidgetMac() const {
  return nullptr;
}

void RenderWidgetHostViewChildFrame::SetActive(bool active) {
}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
  if (frame_connector_) {
    frame_connector_->GetRootRenderWidgetHostView()
        ->ShowDefinitionForSelection();
  }
}

bool RenderWidgetHostViewChildFrame::SupportsSpeech() const {
  return false;
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {
}

bool RenderWidgetHostViewChildFrame::IsSpeaking() const {
  return false;
}

void RenderWidgetHostViewChildFrame::StopSpeaking() {}
#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewChildFrame::RegisterFrameSwappedCallback(
    std::unique_ptr<base::Closure> callback) {
  frame_swapped_callbacks_.push_back(std::move(callback));
}

void RenderWidgetHostViewChildFrame::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  if (!IsSurfaceAvailableForCopy()) {
    // Defer submitting the copy request until after a frame is drawn, at which
    // point we should be guaranteed that the surface is available.
    RegisterFrameSwappedCallback(base::MakeUnique<base::Closure>(base::Bind(
        &RenderWidgetHostViewChildFrame::SubmitSurfaceCopyRequest, AsWeakPtr(),
        src_rect, output_size, callback, preferred_color_type)));
    return;
  }

  SubmitSurfaceCopyRequest(src_rect, output_size, callback,
                           preferred_color_type);
}

void RenderWidgetHostViewChildFrame::SubmitSurfaceCopyRequest(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  DCHECK(IsSurfaceAvailableForCopy());
  DCHECK(support_);

  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(
          base::BindOnce(&CopyFromCompositingSurfaceHasResult, output_size,
                         preferred_color_type, callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);

  support_->RequestCopyOfSurface(std::move(request));
}

bool RenderWidgetHostViewChildFrame::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

void RenderWidgetHostViewChildFrame::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->ReclaimResources(resources);
}

void RenderWidgetHostViewChildFrame::OnBeginFrame(
    const cc::BeginFrameArgs& args) {
  renderer_compositor_frame_sink_->OnBeginFrame(args);
}

void RenderWidgetHostViewChildFrame::OnBeginFramePausedChanged(bool paused) {
  renderer_compositor_frame_sink_->OnBeginFramePausedChanged(paused);
}

void RenderWidgetHostViewChildFrame::SetNeedsBeginFrames(
    bool needs_begin_frames) {
  if (support_)
    support_->SetNeedsBeginFrame(needs_begin_frames);
}

InputEventAckState RenderWidgetHostViewChildFrame::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (input_event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.source_device == blink::kWebGestureDeviceTouchpad &&
        !gesture_event.data.fling_start.velocity_x &&
        !gesture_event.data.fling_start.velocity_y) {
      // Here we indicate that there was no consumer for this event, as
      // otherwise the fling animation system will try to run an animation
      // and will also expect a notification when the fling ends. Since
      // CrOS just uses the GestureFlingStart with zero-velocity as a means
      // of indicating that touchpad scroll has ended, we don't actually want
      // a fling animation.
      // Note: this event handling is modeled on similar code in
      // TenderWidgetHostViewAura::FilterInputEvent().
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    }
  }

  // TODO(mcnee): Allow the root RWHV to consume the child's
  // GestureScrollUpdates. This is needed to prevent the child from consuming
  // them after the root has started an overscroll.
  // See crbug.com/713368

  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

InputEventAckState RenderWidgetHostViewChildFrame::FilterChildGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  // We may be the owner of a RenderWidgetHostViewGuest,
  // so we talk to the root RWHV on its behalf.
  // TODO(mcnee): Remove once MimeHandlerViewGuest is based on OOPIF.
  // See crbug.com/659750
  if (frame_connector_)
    return frame_connector_->GetRootRenderWidgetHostView()
        ->FilterChildGestureEvent(gesture_event);
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

BrowserAccessibilityManager*
RenderWidgetHostViewChildFrame::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  return BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

void RenderWidgetHostViewChildFrame::ClearCompositorSurfaceIfNecessary() {
  if (!support_)
    return;
  support_->EvictCurrentSurface();
  has_frame_ = false;
}

bool RenderWidgetHostViewChildFrame::IsChildFrameForTesting() const {
  return true;
}

viz::SurfaceId RenderWidgetHostViewChildFrame::SurfaceIdForTesting() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

void RenderWidgetHostViewChildFrame::CreateCompositorFrameSinkSupport() {
  if (service_manager::ServiceManagerIsRemote())
    return;

  DCHECK(!support_);
  constexpr bool is_root = false;
  constexpr bool handles_frame_sink_id_invalidation = false;
  constexpr bool needs_sync_points = true;
  support_ = GetHostFrameSinkManager()->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, handles_frame_sink_id_invalidation,
      needs_sync_points);
  if (parent_frame_sink_id_.is_valid()) {
    GetFrameSinkManager()->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                      frame_sink_id_);
  }
  if (host_->needs_begin_frames())
    support_->SetNeedsBeginFrame(true);
}

void RenderWidgetHostViewChildFrame::ResetCompositorFrameSinkSupport() {
  if (!support_)
    return;
  if (parent_frame_sink_id_.is_valid()) {
    GetFrameSinkManager()->UnregisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                        frame_sink_id_);
  }
  support_.reset();
}

bool RenderWidgetHostViewChildFrame::HasEmbedderChanged() {
  return false;
}

bool RenderWidgetHostViewChildFrame::GetSelectionRange(
    gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const TextInputManager::TextSelection* selection =
      text_input_manager_->GetTextSelection(GetFocusedWidget()->GetView());
  if (!selection)
    return false;

  range->set_start(selection->range().start());
  range->set_end(selection->range().end());

  return true;
}

ui::TextInputType RenderWidgetHostViewChildFrame::GetTextInputType() const {
  if (!text_input_manager_)
    return ui::TEXT_INPUT_TYPE_NONE;

  if (text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->type;
  return ui::TEXT_INPUT_TYPE_NONE;
}

gfx::Point RenderWidgetHostViewChildFrame::GetViewOriginInRoot() const {
  if (frame_connector_) {
    auto origin = GetViewBounds().origin() -
                  frame_connector_->GetRootRenderWidgetHostView()
                      ->GetViewBounds()
                      .origin();
    return gfx::Point(origin.x(), origin.y());
  }
  return gfx::Point();
}

}  // namespace content
