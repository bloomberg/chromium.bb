// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_impl.h"

#include <math.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/renderer_host/backing_store_manager.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/immediate_input_router.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/accessibility_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/compositor_util.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/vector2d_conversions.h"
#include "webkit/common/cursors/webcursor.h"
#include "webkit/common/webpreferences.h"

#if defined(TOOLKIT_GTK)
#include "content/browser/renderer_host/backing_store_gtk.h"
#elif defined(OS_MACOSX)
#include "content/browser/renderer_host/backing_store_mac.h"
#elif defined(OS_WIN)
#include "content/common/plugin_constants_win.h"
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTextDirection;

namespace content {
namespace {

bool g_check_for_pending_resize_ack = true;

// How long to (synchronously) wait for the renderer to respond with a
// PaintRect message, when our backing-store is invalid, before giving up and
// returning a null or incorrectly sized backing-store from GetBackingStore.
// This timeout impacts the "choppiness" of our window resize perf.
const int kPaintMsgTimeoutMS = 50;

base::LazyInstance<std::vector<RenderWidgetHost::CreatedCallback> >
g_created_callbacks = LAZY_INSTANCE_INITIALIZER;

}  // namespace


typedef std::pair<int32, int32> RenderWidgetHostID;
typedef base::hash_map<RenderWidgetHostID, RenderWidgetHostImpl*>
    RoutingIDWidgetMap;
static base::LazyInstance<RoutingIDWidgetMap> g_routing_id_widget_map =
    LAZY_INSTANCE_INITIALIZER;

// static
void RenderWidgetHost::RemoveAllBackingStores() {
  BackingStoreManager::RemoveAllBackingStores();
}

// static
size_t RenderWidgetHost::BackingStoreMemorySize() {
  return BackingStoreManager::MemorySize();
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostImpl

RenderWidgetHostImpl::RenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                                           RenderProcessHost* process,
                                           int routing_id)
    : view_(NULL),
      renderer_initialized_(false),
      hung_renderer_delay_ms_(kHungRendererDelayMs),
      delegate_(delegate),
      process_(process),
      routing_id_(routing_id),
      surface_id_(0),
      is_loading_(false),
      is_hidden_(false),
      is_fullscreen_(false),
      is_accelerated_compositing_active_(false),
      repaint_ack_pending_(false),
      resize_ack_pending_(false),
      overdraw_bottom_height_(0.f),
      should_auto_resize_(false),
      waiting_for_screen_rects_ack_(false),
      accessibility_mode_(AccessibilityModeOff),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      in_flight_event_count_(0),
      in_get_backing_store_(false),
      abort_get_backing_store_(false),
      view_being_painted_(false),
      ignore_input_events_(false),
      input_method_active_(false),
      text_direction_updated_(false),
      text_direction_(WebKit::WebTextDirectionLeftToRight),
      text_direction_canceled_(false),
      suppress_next_char_events_(false),
      pending_mouse_lock_request_(false),
      allow_privileged_mouse_lock_(false),
      has_touch_handler_(false),
      weak_factory_(this),
      last_input_number_(0) {
  CHECK(delegate_);
  if (routing_id_ == MSG_ROUTING_NONE) {
    routing_id_ = process_->GetNextRoutingID();
    surface_id_ = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
        process_->GetID(),
        routing_id_);
  } else {
    // TODO(piman): This is a O(N) lookup, where we could forward the
    // information from the RenderWidgetHelper. The problem is that doing so
    // currently leaks outside of content all the way to chrome classes, and
    // would be a layering violation. Since we don't expect more than a few
    // hundreds of RWH, this seems acceptable. Revisit if performance become a
    // problem, for example by tracking in the RenderWidgetHelper the routing id
    // (and surface id) that have been created, but whose RWH haven't yet.
    surface_id_ = GpuSurfaceTracker::Get()->LookupSurfaceForRenderer(
        process_->GetID(),
        routing_id_);
    DCHECK(surface_id_);
  }

  is_threaded_compositing_enabled_ = IsThreadedCompositingEnabled();

  g_routing_id_widget_map.Get().insert(std::make_pair(
      RenderWidgetHostID(process->GetID(), routing_id_), this));
  process_->AddRoute(routing_id_, this);
  // Because the widget initializes as is_hidden_ == false,
  // tell the process host that we're alive.
  process_->WidgetRestored();

  accessibility_mode_ =
      BrowserAccessibilityStateImpl::GetInstance()->accessibility_mode();

  for (size_t i = 0; i < g_created_callbacks.Get().size(); i++)
    g_created_callbacks.Get().at(i).Run(this);

  input_router_.reset(new ImmediateInputRouter(process, this, routing_id_));

#if defined(USE_AURA)
  bool overscroll_enabled = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kOverscrollHistoryNavigation) != "0";
  SetOverscrollControllerEnabled(overscroll_enabled);
#endif
}

RenderWidgetHostImpl::~RenderWidgetHostImpl() {
  SetView(NULL);

  // Clear our current or cached backing store if either remains.
  BackingStoreManager::RemoveBackingStore(this);

  GpuSurfaceTracker::Get()->RemoveSurface(surface_id_);
  surface_id_ = 0;

  process_->RemoveRoute(routing_id_);
  g_routing_id_widget_map.Get().erase(
      RenderWidgetHostID(process_->GetID(), routing_id_));

  if (delegate_)
    delegate_->RenderWidgetDeleted(this);
}

// static
RenderWidgetHost* RenderWidgetHost::FromID(
    int32 process_id,
    int32 routing_id) {
  return RenderWidgetHostImpl::FromID(process_id, routing_id);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::FromID(
    int32 process_id,
    int32 routing_id) {
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  RoutingIDWidgetMap::iterator it = widgets->find(
      RenderWidgetHostID(process_id, routing_id));
  return it == widgets->end() ? NULL : it->second;
}

// static
std::vector<RenderWidgetHost*> RenderWidgetHost::GetRenderWidgetHosts() {
  std::vector<RenderWidgetHost*> hosts;
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  for (RoutingIDWidgetMap::const_iterator it = widgets->begin();
       it != widgets->end();
       ++it) {
    RenderWidgetHost* widget = it->second;

    if (!widget->IsRenderView()) {
      hosts.push_back(widget);
      continue;
    }

    // Add only active RenderViewHosts.
    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (!static_cast<RenderViewHostImpl*>(rvh)->is_swapped_out())
      hosts.push_back(widget);
  }
  return hosts;
}

// static
std::vector<RenderWidgetHost*> RenderWidgetHostImpl::GetAllRenderWidgetHosts() {
  std::vector<RenderWidgetHost*> hosts;
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  for (RoutingIDWidgetMap::const_iterator it = widgets->begin();
       it != widgets->end();
       ++it) {
    hosts.push_back(it->second);
  }
  return hosts;
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::From(RenderWidgetHost* rwh) {
  return rwh->AsRenderWidgetHostImpl();
}

// static
void RenderWidgetHost::AddCreatedCallback(const CreatedCallback& callback) {
  g_created_callbacks.Get().push_back(callback);
}

// static
void RenderWidgetHost::RemoveCreatedCallback(const CreatedCallback& callback) {
  for (size_t i = 0; i < g_created_callbacks.Get().size(); ++i) {
    if (g_created_callbacks.Get().at(i).Equals(callback)) {
      g_created_callbacks.Get().erase(g_created_callbacks.Get().begin() + i);
      return;
    }
  }
}

void RenderWidgetHostImpl::SetView(RenderWidgetHostView* view) {
  view_ = RenderWidgetHostViewPort::FromRWHV(view);

  if (!view_) {
    GpuSurfaceTracker::Get()->SetSurfaceHandle(
        surface_id_, gfx::GLSurfaceHandle());
  }
}

RenderProcessHost* RenderWidgetHostImpl::GetProcess() const {
  return process_;
}

int RenderWidgetHostImpl::GetRoutingID() const {
  return routing_id_;
}

RenderWidgetHostView* RenderWidgetHostImpl::GetView() const {
  return view_;
}

RenderWidgetHostImpl* RenderWidgetHostImpl::AsRenderWidgetHostImpl() {
  return this;
}

gfx::NativeViewId RenderWidgetHostImpl::GetNativeViewId() const {
  if (view_)
    return view_->GetNativeViewId();
  return 0;
}

gfx::GLSurfaceHandle RenderWidgetHostImpl::GetCompositingSurface() {
  if (view_)
    return view_->GetCompositingSurface();
  return gfx::GLSurfaceHandle();
}

void RenderWidgetHostImpl::CompositingSurfaceUpdated() {
  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id_, GetCompositingSurface());
  process_->SurfaceUpdated(surface_id_);
}

void RenderWidgetHostImpl::ResetSizeAndRepaintPendingFlags() {
  resize_ack_pending_ = false;
  if (repaint_ack_pending_) {
    TRACE_EVENT_ASYNC_END0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
  }
  repaint_ack_pending_ = false;
  last_requested_size_.SetSize(0, 0);
}

void RenderWidgetHostImpl::SendScreenRects() {
  if (!renderer_initialized_ || waiting_for_screen_rects_ack_)
    return;

  if (is_hidden_) {
    // On GTK, this comes in for backgrounded tabs. Ignore, to match what
    // happens on Win & Mac, and when the view is shown it'll call this again.
    return;
  }

  if (!view_)
    return;

  last_view_screen_rect_ = view_->GetViewBounds();
  last_window_screen_rect_ = view_->GetBoundsInRootWindow();
  Send(new ViewMsg_UpdateScreenRects(
      GetRoutingID(), last_view_screen_rect_, last_window_screen_rect_));
  if (delegate_)
    delegate_->DidSendScreenRects(this);
  waiting_for_screen_rects_ack_ = true;
}

base::TimeDelta
    RenderWidgetHostImpl::GetSyntheticScrollMessageInterval() const {
  return smooth_scroll_gesture_controller_.GetSyntheticScrollMessageInterval();
}

void RenderWidgetHostImpl::SetOverscrollControllerEnabled(bool enabled) {
  if (!enabled)
    overscroll_controller_.reset();
  else if (!overscroll_controller_)
    overscroll_controller_.reset(new OverscrollController(this));
}

void RenderWidgetHostImpl::SuppressNextCharEvents() {
  suppress_next_char_events_ = true;
}

void RenderWidgetHostImpl::Init() {
  DCHECK(process_->HasConnection());

  renderer_initialized_ = true;

  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id_, GetCompositingSurface());

  // Send the ack along with the information on placement.
  Send(new ViewMsg_CreatingNew_ACK(routing_id_));
  GetProcess()->ResumeRequestsForView(routing_id_);

  WasResized();
}

void RenderWidgetHostImpl::Shutdown() {
  RejectMouseLockOrUnlockIfNecessary();

  if (process_->HasConnection()) {
    // Tell the renderer object to close.
    bool rv = Send(new ViewMsg_Close(routing_id_));
    DCHECK(rv);
  }

  Destroy();
}

bool RenderWidgetHostImpl::IsLoading() const {
  return is_loading_;
}

bool RenderWidgetHostImpl::IsRenderView() const {
  return false;
}

bool RenderWidgetHostImpl::OnMessageReceived(const IPC::Message &msg) {
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderWidgetHostImpl, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateScreenRects_ACK,
                        OnUpdateScreenRectsAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetTooltipText, OnSetTooltipText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PaintAtSize_ACK, OnPaintAtSizeAck)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CompositorSurfaceBuffersSwapped,
                        OnCompositorSurfaceBuffersSwapped)
#endif
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_SwapCompositorFrame,
                                msg_is_ok = OnSwapCompositorFrame(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidOverscroll, OnOverscrolled)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateIsDelayed, OnUpdateIsDelayed)
    IPC_MESSAGE_HANDLER(ViewHostMsg_BeginSmoothScroll, OnBeginSmoothScroll)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Blur, OnBlur)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputTypeChanged,
                        OnTextInputTypeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCancelComposition,
                        OnImeCancelComposition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidActivateAcceleratedCompositing,
                        OnDidActivateAcceleratedCompositing)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse, OnLockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowDisambiguationPopup,
                        OnShowDisambiguationPopup)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WindowlessPluginDummyWindowCreated,
                        OnWindowlessPluginDummyWindowCreated)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WindowlessPluginDummyWindowDestroyed,
                        OnWindowlessPluginDummyWindowDestroyed)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_Snapshot, OnSnapshot)
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCompositionRangeChanged,
                        OnImeCompositionRangeChanged)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!handled && input_router_ && input_router_->OnMessageReceived(msg))
    return true;

  if (!handled && view_ && view_->OnMessageReceived(msg))
    return true;

  if (!msg_is_ok) {
    // The message de-serialization failed. Kill the renderer process.
    RecordAction(UserMetricsAction("BadMessageTerminate_RWH"));
    GetProcess()->ReceivedBadMessage();
  }
  return handled;
}

bool RenderWidgetHostImpl::Send(IPC::Message* msg) {
  if (IPC_MESSAGE_ID_CLASS(msg->type()) == InputMsgStart)
    return input_router_->SendInput(msg);

  return process_->Send(msg);
}

void RenderWidgetHostImpl::WasHidden() {
  is_hidden_ = true;

  // Don't bother reporting hung state when we aren't active.
  StopHangMonitorTimeout();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  Send(new ViewMsg_WasHidden(routing_id_));

  // Tell the RenderProcessHost we were hidden.
  process_->WidgetHidden();

  bool is_visible = false;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));
}

void RenderWidgetHostImpl::WasShown() {
  // When we create the widget, it is created as *not* hidden.
  if (!is_hidden_)
    return;
  is_hidden_ = false;

  SendScreenRects();

  BackingStore* backing_store = BackingStoreManager::Lookup(this);
  // If we already have a backing store for this widget, then we don't need to
  // repaint on restore _unless_ we know that our backing store is invalid.
  // When accelerated compositing is on, we must always repaint, even when
  // the backing store exists.
  bool needs_repainting;
  if (needs_repainting_on_restore_ || !backing_store ||
      is_accelerated_compositing_active()) {
    needs_repainting = true;
    needs_repainting_on_restore_ = false;
  } else {
    needs_repainting = false;
  }
  Send(new ViewMsg_WasShown(routing_id_, needs_repainting));

  process_->WidgetRestored();

  bool is_visible = true;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));

  // It's possible for our size to be out of sync with the renderer. The
  // following is one case that leads to this:
  // 1. WasResized -> Send ViewMsg_Resize to render
  // 2. WasResized -> do nothing as resize_ack_pending_ is true
  // 3. WasHidden
  // 4. OnUpdateRect from (1) processed. Does NOT invoke WasResized as view
  //    is hidden. Now renderer/browser out of sync with what they think size
  //    is.
  // By invoking WasResized the renderer is updated as necessary. WasResized
  // does nothing if the sizes are already in sync.
  //
  // TODO: ideally ViewMsg_WasShown would take a size. This way, the renderer
  // could handle both the restore and resize at once. This isn't that big a
  // deal as RenderWidget::WasShown delays updating, so that the resize from
  // WasResized is usually processed before the renderer is painted.
  WasResized();
}

void RenderWidgetHostImpl::WasResized() {
  if (resize_ack_pending_ || !process_->HasConnection() || !view_ ||
      !renderer_initialized_ || should_auto_resize_) {
    return;
  }

  gfx::Rect view_bounds = view_->GetViewBounds();
  gfx::Size new_size(view_bounds.size());

  gfx::Size old_physical_backing_size = physical_backing_size_;
  physical_backing_size_ = view_->GetPhysicalBackingSize();
  bool was_fullscreen = is_fullscreen_;
  is_fullscreen_ = IsFullscreen();
  float old_overdraw_bottom_height = overdraw_bottom_height_;
  overdraw_bottom_height_ = view_->GetOverdrawBottomHeight();

  bool size_changed = new_size != last_requested_size_;
  bool side_payload_changed =
      !screen_info_.get() ||
      old_physical_backing_size != physical_backing_size_ ||
      was_fullscreen != is_fullscreen_ ||
      old_overdraw_bottom_height != overdraw_bottom_height_;

  if (!size_changed && !side_payload_changed)
    return;

  if (!screen_info_) {
    screen_info_.reset(new WebKit::WebScreenInfo);
    GetWebScreenInfo(screen_info_.get());
  }

  // We don't expect to receive an ACK when the requested size or the physical
  // backing size is empty, or when the main viewport size didn't change.
  if (!new_size.IsEmpty() && !physical_backing_size_.IsEmpty() && size_changed)
    resize_ack_pending_ = g_check_for_pending_resize_ack;

  ViewMsg_Resize_Params params;
  params.screen_info = *screen_info_;
  params.new_size = new_size;
  params.physical_backing_size = physical_backing_size_;
  params.overdraw_bottom_height = overdraw_bottom_height_;
  params.resizer_rect = GetRootWindowResizerRect();
  params.is_fullscreen = is_fullscreen_;
  if (!Send(new ViewMsg_Resize(routing_id_, params))) {
    resize_ack_pending_ = false;
  } else {
    last_requested_size_ = new_size;
  }
}

void RenderWidgetHostImpl::ResizeRectChanged(const gfx::Rect& new_rect) {
  Send(new ViewMsg_ChangeResizeRect(routing_id_, new_rect));
}

void RenderWidgetHostImpl::GotFocus() {
  Focus();
}

void RenderWidgetHostImpl::Focus() {
  Send(new InputMsg_SetFocus(routing_id_, true));
}

void RenderWidgetHostImpl::Blur() {
  // If there is a pending mouse lock request, we don't want to reject it at
  // this point. The user can switch focus back to this view and approve the
  // request later.
  if (IsMouseLocked())
    view_->UnlockMouse();

  // If there is a pending overscroll, then that should be cancelled.
  if (overscroll_controller_)
    overscroll_controller_->Cancel();

  Send(new InputMsg_SetFocus(routing_id_, false));
}

void RenderWidgetHostImpl::LostCapture() {
  Send(new InputMsg_MouseCaptureLost(routing_id_));
}

void RenderWidgetHostImpl::SetActive(bool active) {
  Send(new ViewMsg_SetActive(routing_id_, active));
}

void RenderWidgetHostImpl::LostMouseLock() {
  Send(new ViewMsg_MouseLockLost(routing_id_));
}

void RenderWidgetHostImpl::ViewDestroyed() {
  RejectMouseLockOrUnlockIfNecessary();

  // TODO(evanm): tracking this may no longer be necessary;
  // eliminate this function if so.
  SetView(NULL);
}

void RenderWidgetHostImpl::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  if (!view_)
    return;
  view_->SetIsLoading(is_loading);
}

void RenderWidgetHostImpl::CopyFromBackingStore(
    const gfx::Rect& src_subrect,
    const gfx::Size& accelerated_dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  if (view_ && is_accelerated_compositing_active_) {
    TRACE_EVENT0("browser",
        "RenderWidgetHostImpl::CopyFromBackingStore::FromCompositingSurface");
    gfx::Rect accelerated_copy_rect = src_subrect.IsEmpty() ?
        gfx::Rect(view_->GetViewBounds().size()) : src_subrect;
    view_->CopyFromCompositingSurface(accelerated_copy_rect,
                                      accelerated_dst_size,
                                      callback);
    return;
  }

  BackingStore* backing_store = GetBackingStore(false);
  if (!backing_store) {
    callback.Run(false, SkBitmap());
    return;
  }

  TRACE_EVENT0("browser",
      "RenderWidgetHostImpl::CopyFromBackingStore::FromBackingStore");
  gfx::Rect copy_rect = src_subrect.IsEmpty() ?
      gfx::Rect(backing_store->size()) : src_subrect;
  // When the result size is equal to the backing store size, copy from the
  // backing store directly to the output canvas.
  skia::PlatformBitmap output;
  bool result = backing_store->CopyFromBackingStore(copy_rect, &output);
  callback.Run(result, output.GetBitmap());
}

#if defined(TOOLKIT_GTK)
bool RenderWidgetHostImpl::CopyFromBackingStoreToGtkWindow(
    const gfx::Rect& dest_rect, GdkWindow* target) {
  BackingStore* backing_store = GetBackingStore(false);
  if (!backing_store)
    return false;
  (static_cast<BackingStoreGtk*>(backing_store))->PaintToRect(
      dest_rect, target);
  return true;
}
#elif defined(OS_MACOSX)
gfx::Size RenderWidgetHostImpl::GetBackingStoreSize() {
  BackingStore* backing_store = GetBackingStore(false);
  return backing_store ? backing_store->size() : gfx::Size();
}

bool RenderWidgetHostImpl::CopyFromBackingStoreToCGContext(
    const CGRect& dest_rect, CGContextRef target) {
  BackingStore* backing_store = GetBackingStore(false);
  if (!backing_store)
    return false;
  (static_cast<BackingStoreMac*>(backing_store))->
      CopyFromBackingStoreToCGContext(dest_rect, target);
  return true;
}
#endif

void RenderWidgetHostImpl::PaintAtSize(TransportDIB::Handle dib_handle,
                                       int tag,
                                       const gfx::Size& page_size,
                                       const gfx::Size& desired_size) {
  // Ask the renderer to create a bitmap regardless of whether it's
  // hidden, being resized, redrawn, etc.  It resizes the web widget
  // to the page_size and then scales it to the desired_size.
  Send(new ViewMsg_PaintAtSize(routing_id_, dib_handle, tag,
                               page_size, desired_size));
}

bool RenderWidgetHostImpl::TryGetBackingStore(const gfx::Size& desired_size,
                                              BackingStore** backing_store) {
  // Check if the view has an accelerated surface of the desired size.
  if (view_->HasAcceleratedSurface(desired_size)) {
    *backing_store = NULL;
    return true;
  }

  // Check for a software backing store of the desired size.
  *backing_store = BackingStoreManager::GetBackingStore(this, desired_size);
  return !!*backing_store;
}

BackingStore* RenderWidgetHostImpl::GetBackingStore(bool force_create) {
  if (!view_)
    return NULL;

  // The view_size will be current_size_ for auto-sized views and otherwise the
  // size of the view_. (For auto-sized views, current_size_ is updated during
  // UpdateRect messages.)
  gfx::Size view_size = current_size_;
  if (!should_auto_resize_) {
    // Get the desired size from the current view bounds.
    gfx::Rect view_rect = view_->GetViewBounds();
    if (view_rect.IsEmpty())
      return NULL;
    view_size = view_rect.size();
  }

  TRACE_EVENT2("renderer_host", "RenderWidgetHostImpl::GetBackingStore",
               "width", base::IntToString(view_size.width()),
               "height", base::IntToString(view_size.height()));

  // We should not be asked to paint while we are hidden.  If we are hidden,
  // then it means that our consumer failed to call WasShown. If we're not
  // force creating the backing store, it's OK since we can feel free to give
  // out our cached one if we have it.
  DCHECK(!is_hidden_ || !force_create) <<
      "GetBackingStore called while hidden!";

  // We should never be called recursively; this can theoretically lead to
  // infinite recursion and almost certainly leads to lower performance.
  DCHECK(!in_get_backing_store_) << "GetBackingStore called recursively!";
  base::AutoReset<bool> auto_reset_in_get_backing_store(
      &in_get_backing_store_, true);

  // We might have a cached backing store that we can reuse!
  BackingStore* backing_store = NULL;
  if (TryGetBackingStore(view_size, &backing_store) || !force_create)
    return backing_store;

  // We do not have a suitable backing store in the cache, so send out a
  // request to the renderer to paint the view if required.
  if (!repaint_ack_pending_ && !resize_ack_pending_ && !view_being_painted_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    TRACE_EVENT_ASYNC_BEGIN0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }

  TimeDelta max_delay = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
  TimeTicks end_time = TimeTicks::Now() + max_delay;
  do {
    TRACE_EVENT0("renderer_host", "GetBackingStore::WaitForUpdate");

#if defined(OS_MACOSX)
    view_->AboutToWaitForBackingStoreMsg();
#endif

    // When we have asked the RenderWidget to resize, and we are still waiting
    // on a response, block for a little while to see if we can't get a response
    // before returning the old (incorrectly sized) backing store.
    IPC::Message msg;
    if (process_->WaitForBackingStoreMsg(routing_id_, max_delay, &msg)) {
      OnMessageReceived(msg);

      // For auto-resized views, current_size_ determines the view_size and it
      // may have changed during the handling of an UpdateRect message.
      if (should_auto_resize_)
        view_size = current_size_;

      // Break now if we got a backing store or accelerated surface of the
      // correct size.
      if (TryGetBackingStore(view_size, &backing_store) ||
          abort_get_backing_store_) {
        abort_get_backing_store_ = false;
        return backing_store;
      }
    } else {
      TRACE_EVENT0("renderer_host", "GetBackingStore::Timeout");
      break;
    }

    // Loop if we still have time left and haven't gotten a properly sized
    // BackingStore yet. This is necessary to support the GPU path which
    // typically has multiple frames pipelined -- we may need to skip one or two
    // BackingStore messages to get to the latest.
    max_delay = end_time - TimeTicks::Now();
  } while (max_delay > TimeDelta::FromSeconds(0));

  // We have failed to get a backing store of view_size. Fall back on
  // current_size_ to avoid a white flash while resizing slow pages.
  if (view_size != current_size_)
    TryGetBackingStore(current_size_, &backing_store);
  return backing_store;
}

BackingStore* RenderWidgetHostImpl::AllocBackingStore(const gfx::Size& size) {
  if (!view_)
    return NULL;
  return view_->AllocBackingStore(size);
}

void RenderWidgetHostImpl::DonePaintingToBackingStore() {
  Send(new ViewMsg_UpdateRect_ACK(GetRoutingID()));
}

void RenderWidgetHostImpl::ScheduleComposite() {
  if (is_hidden_ || !is_accelerated_compositing_active_ ||
      current_size_.IsEmpty()) {
      return;
  }

  // Send out a request to the renderer to paint the view if required.
  if (!repaint_ack_pending_ && !resize_ack_pending_ && !view_being_painted_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    TRACE_EVENT_ASYNC_BEGIN0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    Send(new ViewMsg_Repaint(routing_id_, current_size_));
  }
}

void RenderWidgetHostImpl::StartHangMonitorTimeout(TimeDelta delay) {
  if (!GetProcess()->IsGuest() && CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    return;
  }

  // Set time_when_considered_hung_ if it's null. Also, update
  // time_when_considered_hung_ if the caller's request is sooner than the
  // existing one. This will have the side effect that the existing timeout will
  // be forgotten.
  Time requested_end_time = Time::Now() + delay;
  if (time_when_considered_hung_.is_null() ||
      time_when_considered_hung_ > requested_end_time)
    time_when_considered_hung_ = requested_end_time;

  // If we already have a timer with the same or shorter duration, then we can
  // wait for it to finish.
  if (hung_renderer_timer_.IsRunning() &&
      hung_renderer_timer_.GetCurrentDelay() <= delay) {
    // If time_when_considered_hung_ was null, this timer may fire early.
    // CheckRendererIsUnresponsive handles that by calling
    // StartHangMonitorTimeout with the remaining time.
    // If time_when_considered_hung_ was non-null, it means we still haven't
    // heard from the renderer so we leave time_when_considered_hung_ as is.
    return;
  }

  // Either the timer is not yet running, or we need to adjust the timer to
  // fire sooner.
  time_when_considered_hung_ = requested_end_time;
  hung_renderer_timer_.Stop();
  hung_renderer_timer_.Start(FROM_HERE, delay, this,
      &RenderWidgetHostImpl::CheckRendererIsUnresponsive);
}

void RenderWidgetHostImpl::RestartHangMonitorTimeout() {
  // Setting to null will cause StartHangMonitorTimeout to restart the timer.
  time_when_considered_hung_ = Time();
  StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(hung_renderer_delay_ms_));
}

void RenderWidgetHostImpl::StopHangMonitorTimeout() {
  time_when_considered_hung_ = Time();
  RendererIsResponsive();
  // We do not bother to stop the hung_renderer_timer_ here in case it will be
  // started again shortly, which happens to be the common use case.
}

void RenderWidgetHostImpl::EnableFullAccessibilityMode() {
  SetAccessibilityMode(AccessibilityModeComplete);
}

static WebGestureEvent MakeGestureEvent(WebInputEvent::Type type,
                                        double timestamp_seconds,
                                        int x,
                                        int y,
                                        int modifiers) {
  WebGestureEvent result;

  result.type = type;
  result.x = x;
  result.y = y;
  result.sourceDevice = WebGestureEvent::Touchscreen;
  result.timeStampSeconds = timestamp_seconds;
  result.modifiers = modifiers;

  return result;
}

void RenderWidgetHostImpl::SimulateTouchGestureWithMouse(
    const WebMouseEvent& mouse_event) {
  int x = mouse_event.x, y = mouse_event.y;
  float dx = mouse_event.movementX, dy = mouse_event.movementY;
  static int startX = 0, startY = 0;

  switch (mouse_event.button) {
    case WebMouseEvent::ButtonLeft:
      if (mouse_event.type == WebInputEvent::MouseDown) {
        startX = x;
        startY = y;
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollBegin, mouse_event.timeStampSeconds,
            x, y, 0));
      }
      if (dx != 0 || dy != 0) {
        WebGestureEvent event = MakeGestureEvent(
            WebInputEvent::GestureScrollUpdate, mouse_event.timeStampSeconds,
            x, y, 0);
        event.data.scrollUpdate.deltaX = dx;
        event.data.scrollUpdate.deltaY = dy;
        ForwardGestureEvent(event);
      }
      if (mouse_event.type == WebInputEvent::MouseUp) {
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollEnd, mouse_event.timeStampSeconds,
            x, y, 0));
      }
      break;
    case WebMouseEvent::ButtonMiddle:
      if (mouse_event.type == WebInputEvent::MouseDown) {
        startX = x;
        startY = y;
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureTapDown, mouse_event.timeStampSeconds,
            x, y, 0));
      }
      if (mouse_event.type == WebInputEvent::MouseUp) {
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureTap, mouse_event.timeStampSeconds,
            x, y, 0));
      }
      break;
    case WebMouseEvent::ButtonRight:
      if (mouse_event.type == WebInputEvent::MouseDown) {
        startX = x;
        startY = y;
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollBegin, mouse_event.timeStampSeconds,
            x, y, 0));
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GesturePinchBegin, mouse_event.timeStampSeconds,
            x, y, 0));
      }
      if (dx != 0 || dy != 0) {
        dx = pow(dy < 0 ? 0.998f : 1.002f, fabs(dy));
        WebGestureEvent event = MakeGestureEvent(
            WebInputEvent::GesturePinchUpdate, mouse_event.timeStampSeconds,
            startX, startY, 0);
        event.data.pinchUpdate.scale = dx;
        ForwardGestureEvent(event);
      }
      if (mouse_event.type == WebInputEvent::MouseUp) {
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GesturePinchEnd, mouse_event.timeStampSeconds,
            x, y, 0));
        ForwardGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollEnd, mouse_event.timeStampSeconds,
            x, y, 0));
      }
      break;
    case WebMouseEvent::ButtonNone:
      break;
  }
}

void RenderWidgetHostImpl::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  ForwardMouseEventWithLatencyInfo(
      MouseEventWithLatencyInfo(mouse_event,
                                CreateRWHLatencyInfoIfNotExist(NULL)));
}

void RenderWidgetHostImpl::ForwardMouseEventWithLatencyInfo(
    const MouseEventWithLatencyInfo& mouse_event) {
  TRACE_EVENT2("input", "RenderWidgetHostImpl::ForwardMouseEvent",
               "x", mouse_event.event.x, "y", mouse_event.event.y);
  input_router_->SendMouseEvent(mouse_event);
}

void RenderWidgetHostImpl::OnPointerEventActivate() {
}

void RenderWidgetHostImpl::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  ForwardWheelEventWithLatencyInfo(
      MouseWheelEventWithLatencyInfo(wheel_event,
                                     CreateRWHLatencyInfoIfNotExist(NULL)));
}

void RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardWheelEvent");
  input_router_->SendWheelEvent(wheel_event);
}

void RenderWidgetHostImpl::ForwardGestureEvent(
    const WebKit::WebGestureEvent& gesture_event) {
  ForwardGestureEventWithLatencyInfo(gesture_event, ui::LatencyInfo());
}

void RenderWidgetHostImpl::ForwardGestureEventWithLatencyInfo(
    const WebKit::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardGestureEvent");
  // Early out if necessary, prior to performing latency logic.
  if (IgnoreInputEvents())
    return;

  ui::LatencyInfo latency_info = CreateRWHLatencyInfoIfNotExist(&ui_latency);

  if (gesture_event.type == WebKit::WebInputEvent::GestureScrollUpdate) {
    latency_info.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT,
        GetLatencyComponentId(),
        ++last_input_number_);

    // Make a copy of the INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT with a
    // different name INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
    // So we can track the latency specifically for scroll update events.
    ui::LatencyInfo::LatencyComponent original_component;
    if (latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                 0,
                                 &original_component)) {
      latency_info.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          GetLatencyComponentId(),
          original_component.sequence_number,
          original_component.event_time,
          original_component.event_count);
    }
  }

  GestureEventWithLatencyInfo gesture_with_latency(gesture_event, latency_info);
  input_router_->SendGestureEvent(gesture_with_latency);
}

void RenderWidgetHostImpl::ForwardTouchEventWithLatencyInfo(
      const WebKit::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardTouchEvent");
  ui::LatencyInfo latency_info = CreateRWHLatencyInfoIfNotExist(&ui_latency);
  TouchEventWithLatencyInfo touch_with_latency(touch_event, latency_info);
  input_router_->SendTouchEvent(touch_with_latency);
}

void RenderWidgetHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardKeyboardEvent");
  input_router_->SendKeyboardEvent(key_event,
                                   CreateRWHLatencyInfoIfNotExist(NULL));
}

void RenderWidgetHostImpl::SendCursorVisibilityState(bool is_visible) {
  Send(new InputMsg_CursorVisibilityChange(GetRoutingID(), is_visible));
}

int64 RenderWidgetHostImpl::GetLatencyComponentId() {
  return GetRoutingID() | (static_cast<int64>(GetProcess()->GetID()) << 32);
}

// static
void RenderWidgetHostImpl::DisableResizeAckCheckForTesting() {
  g_check_for_pending_resize_ack = false;
}

ui::LatencyInfo RenderWidgetHostImpl::CreateRWHLatencyInfoIfNotExist(
    const ui::LatencyInfo* original) {
  ui::LatencyInfo info;
  if (original)
    info = *original;
  // In Aura, gesture event will already carry its original touch event's
  // INPUT_EVENT_LATENCY_RWH_COMPONENT.
  if (!info.FindLatency(ui::INPUT_EVENT_LATENCY_RWH_COMPONENT,
                        GetLatencyComponentId(),
                        NULL)) {
    info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_RWH_COMPONENT,
                          GetLatencyComponentId(),
                          ++last_input_number_);
  }
  return info;
}


void RenderWidgetHostImpl::AddKeyboardListener(KeyboardListener* listener) {
  keyboard_listeners_.AddObserver(listener);
}

void RenderWidgetHostImpl::RemoveKeyboardListener(
    KeyboardListener* listener) {
  // Ensure that the element is actually an observer.
  DCHECK(keyboard_listeners_.HasObserver(listener));
  keyboard_listeners_.RemoveObserver(listener);
}

void RenderWidgetHostImpl::GetWebScreenInfo(WebKit::WebScreenInfo* result) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::GetWebScreenInfo");
  if (GetView())
    static_cast<RenderWidgetHostViewPort*>(GetView())->GetScreenInfo(result);
  else
    RenderWidgetHostViewPort::GetDefaultScreenInfo(result);
}

const NativeWebKeyboardEvent*
    RenderWidgetHostImpl::GetLastKeyboardEvent() const {
  return input_router_->GetLastKeyboardEvent();
}

void RenderWidgetHostImpl::NotifyScreenInfoChanged() {
  // The resize message (which may not happen immediately) will carry with it
  // the screen info as well as the new size (if the screen has changed scale
  // factor).
  InvalidateScreenInfo();
  WasResized();
}

void RenderWidgetHostImpl::InvalidateScreenInfo() {
  screen_info_.reset();
}

void RenderWidgetHostImpl::GetSnapshotFromRenderer(
    const gfx::Rect& src_subrect,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  TRACE_EVENT0("browser", "RenderWidgetHostImpl::GetSnapshotFromRenderer");
  pending_snapshots_.push(callback);

  gfx::Rect copy_rect = src_subrect.IsEmpty() ?
      gfx::Rect(view_->GetViewBounds().size()) : src_subrect;

  gfx::Rect copy_rect_in_pixel = ConvertViewRectToPixel(view_, copy_rect);
  Send(new ViewMsg_Snapshot(GetRoutingID(), copy_rect_in_pixel));
}

void RenderWidgetHostImpl::OnSnapshot(bool success,
                                    const SkBitmap& bitmap) {
  if (pending_snapshots_.size() == 0) {
    LOG(ERROR) << "RenderWidgetHostImpl::OnSnapshot: "
                  "Received a snapshot that was not requested.";
    return;
  }

  base::Callback<void(bool, const SkBitmap&)> callback =
      pending_snapshots_.front();
  pending_snapshots_.pop();

  if (!success) {
    callback.Run(success, SkBitmap());
    return;
  }

  callback.Run(success, bitmap);
}

void RenderWidgetHostImpl::UpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  Send(new ViewMsg_UpdateVSyncParameters(GetRoutingID(), timebase, interval));
}

void RenderWidgetHostImpl::RendererExited(base::TerminationStatus status,
                                          int exit_code) {
  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_initialized_ = false;

  waiting_for_screen_rects_ack_ = false;

  // Reset to ensure that input routing works with a new renderer.
  input_router_.reset(new ImmediateInputRouter(process_, this, routing_id_));

  if (overscroll_controller_)
    overscroll_controller_->Reset();

 // Must reset these to ensure that keyboard events work with a new renderer.
  suppress_next_char_events_ = false;

  // Reset some fields in preparation for recovering from a crash.
  ResetSizeAndRepaintPendingFlags();
  current_size_.SetSize(0, 0);
  is_hidden_ = false;
  is_accelerated_compositing_active_ = false;

  // Reset this to ensure the hung renderer mechanism is working properly.
  in_flight_event_count_ = 0;

  if (view_) {
    GpuSurfaceTracker::Get()->SetSurfaceHandle(surface_id_,
                                               gfx::GLSurfaceHandle());
    view_->RenderProcessGone(status, exit_code);
    view_ = NULL;  // The View should be deleted by RenderProcessGone.
  }

  BackingStoreManager::RemoveBackingStore(this);
}

void RenderWidgetHostImpl::UpdateTextDirection(WebTextDirection direction) {
  text_direction_updated_ = true;
  text_direction_ = direction;
}

void RenderWidgetHostImpl::CancelUpdateTextDirection() {
  if (text_direction_updated_)
    text_direction_canceled_ = true;
}

void RenderWidgetHostImpl::NotifyTextDirection() {
  if (text_direction_updated_) {
    if (!text_direction_canceled_)
      Send(new ViewMsg_SetTextDirection(GetRoutingID(), text_direction_));
    text_direction_updated_ = false;
    text_direction_canceled_ = false;
  }
}

void RenderWidgetHostImpl::SetInputMethodActive(bool activate) {
  input_method_active_ = activate;
  Send(new ViewMsg_SetInputMethodActive(GetRoutingID(), activate));
}

void RenderWidgetHostImpl::ImeSetComposition(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  Send(new ViewMsg_ImeSetComposition(
            GetRoutingID(), text, underlines, selection_start, selection_end));
}

void RenderWidgetHostImpl::ImeConfirmComposition(
    const string16& text,
    const ui::Range& replacement_range,
    bool keep_selection) {
  Send(new ViewMsg_ImeConfirmComposition(
        GetRoutingID(), text, replacement_range, keep_selection));
}

void RenderWidgetHostImpl::ImeCancelComposition() {
  Send(new ViewMsg_ImeSetComposition(GetRoutingID(), string16(),
            std::vector<WebKit::WebCompositionUnderline>(), 0, 0));
}

void RenderWidgetHostImpl::ExtendSelectionAndDelete(
    size_t before,
    size_t after) {
  Send(new ViewMsg_ExtendSelectionAndDelete(GetRoutingID(), before, after));
}

gfx::Rect RenderWidgetHostImpl::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

void RenderWidgetHostImpl::RequestToLockMouse(bool user_gesture,
                                              bool last_unlocked_by_target) {
  // Directly reject to lock the mouse. Subclass can override this method to
  // decide whether to allow mouse lock or not.
  GotResponseToLockMouseRequest(false);
}

void RenderWidgetHostImpl::RejectMouseLockOrUnlockIfNecessary() {
  DCHECK(!pending_mouse_lock_request_ || !IsMouseLocked());
  if (pending_mouse_lock_request_) {
    pending_mouse_lock_request_ = false;
    Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
  } else if (IsMouseLocked()) {
    view_->UnlockMouse();
  }
}

bool RenderWidgetHostImpl::IsMouseLocked() const {
  return view_ ? view_->IsMouseLocked() : false;
}

bool RenderWidgetHostImpl::IsFullscreen() const {
  return false;
}

void RenderWidgetHostImpl::SetShouldAutoResize(bool enable) {
  should_auto_resize_ = enable;
}

bool RenderWidgetHostImpl::IsInOverscrollGesture() const {
  return overscroll_controller_.get() &&
         overscroll_controller_->overscroll_mode() != OVERSCROLL_NONE;
}

void RenderWidgetHostImpl::Destroy() {
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // Tell the view to die.
  // Note that in the process of the view shutting down, it can call a ton
  // of other messages on us.  So if you do any other deinitialization here,
  // do it after this call to view_->Destroy().
  if (view_)
    view_->Destroy();

  delete this;
}

void RenderWidgetHostImpl::CheckRendererIsUnresponsive() {
  // If we received a call to StopHangMonitorTimeout.
  if (time_when_considered_hung_.is_null())
    return;

  // If we have not waited long enough, then wait some more.
  Time now = Time::Now();
  if (now < time_when_considered_hung_) {
    StartHangMonitorTimeout(time_when_considered_hung_ - now);
    return;
  }

  // OK, looks like we have a hung renderer!
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_HANG,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());
  is_unresponsive_ = true;
  NotifyRendererUnresponsive();
}

void RenderWidgetHostImpl::RendererIsResponsive() {
  if (is_unresponsive_) {
    is_unresponsive_ = false;
    NotifyRendererResponsive();
  }
}

void RenderWidgetHostImpl::OnRenderViewReady() {
  SendScreenRects();
  WasResized();
}

void RenderWidgetHostImpl::OnRenderProcessGone(int status, int exit_code) {
  // TODO(evanm): This synchronously ends up calling "delete this".
  // Is that really what we want in response to this message?  I'm matching
  // previous behavior of the code here.
  Destroy();
}

void RenderWidgetHostImpl::OnClose() {
  Shutdown();
}

void RenderWidgetHostImpl::OnSetTooltipText(
    const string16& tooltip_text,
    WebTextDirection text_direction_hint) {
  // First, add directionality marks around tooltip text if necessary.
  // A naive solution would be to simply always wrap the text. However, on
  // windows, Unicode directional embedding characters can't be displayed on
  // systems that lack RTL fonts and are instead displayed as empty squares.
  //
  // To get around this we only wrap the string when we deem it necessary i.e.
  // when the locale direction is different than the tooltip direction hint.
  //
  // Currently, we use element's directionality as the tooltip direction hint.
  // An alternate solution would be to set the overall directionality based on
  // trying to detect the directionality from the tooltip text rather than the
  // element direction.  One could argue that would be a preferable solution
  // but we use the current approach to match Fx & IE's behavior.
  string16 wrapped_tooltip_text = tooltip_text;
  if (!tooltip_text.empty()) {
    if (text_direction_hint == WebKit::WebTextDirectionLeftToRight) {
      // Force the tooltip to have LTR directionality.
      wrapped_tooltip_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_tooltip_text);
    } else if (text_direction_hint == WebKit::WebTextDirectionRightToLeft &&
               !base::i18n::IsRTL()) {
      // Force the tooltip to have RTL directionality.
      base::i18n::WrapStringWithRTLFormatting(&wrapped_tooltip_text);
    }
  }
  if (GetView())
    view_->SetTooltipText(wrapped_tooltip_text);
}

void RenderWidgetHostImpl::OnUpdateScreenRectsAck() {
  waiting_for_screen_rects_ack_ = false;
  if (!view_)
    return;

  if (view_->GetViewBounds() == last_view_screen_rect_ &&
      view_->GetBoundsInRootWindow() == last_window_screen_rect_) {
    return;
  }

  SendScreenRects();
}

void RenderWidgetHostImpl::OnRequestMove(const gfx::Rect& pos) {
  // Note that we ignore the position.
  if (view_) {
    view_->SetBounds(pos);
    Send(new ViewMsg_Move_ACK(routing_id_));
  }
}

void RenderWidgetHostImpl::OnPaintAtSizeAck(int tag, const gfx::Size& size) {
  std::pair<int, gfx::Size> details = std::make_pair(tag, size);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
      Source<RenderWidgetHost>(this),
      Details<std::pair<int, gfx::Size> >(&details));
}

#if defined(OS_MACOSX)
void RenderWidgetHostImpl::OnCompositorSurfaceBuffersSwapped(
      const ViewHostMsg_CompositorSurfaceBuffersSwapped_Params& params) {
  TRACE_EVENT0("renderer_host",
               "RenderWidgetHostImpl::OnCompositorSurfaceBuffersSwapped");
  if (!view_) {
    AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
    ack_params.sync_point = 0;
    RenderWidgetHostImpl::AcknowledgeBufferPresent(params.route_id,
                                                   params.gpu_process_host_id,
                                                   ack_params);
    return;
  }
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params gpu_params;
  gpu_params.surface_id = params.surface_id;
  gpu_params.surface_handle = params.surface_handle;
  gpu_params.route_id = params.route_id;
  gpu_params.size = params.size;
  gpu_params.scale_factor = params.scale_factor;
  gpu_params.latency_info = params.latency_info;
  view_->AcceleratedSurfaceBuffersSwapped(gpu_params,
                                          params.gpu_process_host_id);
  view_->DidReceiveRendererFrame();
}
#endif  // OS_MACOSX

bool RenderWidgetHostImpl::OnSwapCompositorFrame(
    const IPC::Message& message) {
  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return false;
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  uint32 output_surface_id = param.a;
  param.b.AssignTo(frame.get());

  if (view_) {
    view_->OnSwapCompositorFrame(output_surface_id, frame.Pass());
    view_->DidReceiveRendererFrame();
  } else {
    cc::CompositorFrameAck ack;
    if (frame->gl_frame_data) {
      ack.gl_frame_data = frame->gl_frame_data.Pass();
      ack.gl_frame_data->sync_point = 0;
    } else if (frame->delegated_frame_data) {
      ack.resources.swap(frame->delegated_frame_data->resource_list);
    } else if (frame->software_frame_data) {
      ack.last_software_frame_id = frame->software_frame_data->id;
    }
    SendSwapCompositorFrameAck(routing_id_, process_->GetID(),
                               output_surface_id,  ack);
  }
  return true;
}

void RenderWidgetHostImpl::OnOverscrolled(
    gfx::Vector2dF accumulated_overscroll,
    gfx::Vector2dF current_fling_velocity) {
  if (view_)
    view_->OnOverscrolled(accumulated_overscroll, current_fling_velocity);
}

void RenderWidgetHostImpl::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::OnUpdateRect");
  TimeTicks paint_start = TimeTicks::Now();

  // Update our knowledge of the RenderWidget's size.
  current_size_ = params.view_size;
  // Update our knowledge of the RenderWidget's scroll offset.
  last_scroll_offset_ = params.scroll_offset;

  bool is_resize_ack =
      ViewHostMsg_UpdateRect_Flags::is_resize_ack(params.flags);

  // resize_ack_pending_ needs to be cleared before we call DidPaintRect, since
  // that will end up reaching GetBackingStore.
  if (is_resize_ack) {
    DCHECK(!g_check_for_pending_resize_ack || resize_ack_pending_);
    resize_ack_pending_ = false;
  }

  bool is_repaint_ack =
      ViewHostMsg_UpdateRect_Flags::is_repaint_ack(params.flags);
  if (is_repaint_ack) {
    DCHECK(repaint_ack_pending_);
    TRACE_EVENT_ASYNC_END0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    repaint_ack_pending_ = false;
    TimeDelta delta = TimeTicks::Now() - repaint_start_time_;
    UMA_HISTOGRAM_TIMES("MPArch.RWH_RepaintDelta", delta);
  }

  DCHECK(!params.view_size.IsEmpty());

  bool was_async = false;

  // If this is a GPU UpdateRect, params.bitmap is invalid and dib will be NULL.
  TransportDIB* dib = process_->GetTransportDIB(params.bitmap);

  // If gpu process does painting, scroll_rect and copy_rects are always empty
  // and backing store is never used.
  if (dib) {
    DCHECK(!params.bitmap_rect.IsEmpty());
    gfx::Size pixel_size = gfx::ToFlooredSize(
        gfx::ScaleSize(params.bitmap_rect.size(), params.scale_factor));
    const size_t size = pixel_size.height() * pixel_size.width() * 4;
    if (dib->size() < size) {
      DLOG(WARNING) << "Transport DIB too small for given rectangle";
      RecordAction(UserMetricsAction("BadMessageTerminate_RWH1"));
      GetProcess()->ReceivedBadMessage();
    } else {
      // Scroll the backing store.
      if (!params.scroll_rect.IsEmpty()) {
        ScrollBackingStoreRect(params.scroll_delta,
                               params.scroll_rect,
                               params.view_size);
      }

      // Paint the backing store. This will update it with the
      // renderer-supplied bits. The view will read out of the backing store
      // later to actually draw to the screen.
      was_async = PaintBackingStoreRect(
          params.bitmap,
          params.bitmap_rect,
          params.copy_rects,
          params.view_size,
          params.scale_factor,
          base::Bind(&RenderWidgetHostImpl::DidUpdateBackingStore,
                     weak_factory_.GetWeakPtr(), params, paint_start));
    }
  }

  if (!was_async) {
    DidUpdateBackingStore(params, paint_start);
  }

  if (should_auto_resize_) {
    bool post_callback = new_auto_size_.IsEmpty();
    new_auto_size_ = params.view_size;
    if (post_callback) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&RenderWidgetHostImpl::DelayedAutoResized,
                     weak_factory_.GetWeakPtr()));
    }
  }

  // Log the time delta for processing a paint message. On platforms that don't
  // support asynchronous painting, this is equivalent to
  // MPArch.RWH_TotalPaintTime.
  TimeDelta delta = TimeTicks::Now() - paint_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgUpdateRect", delta);
}

void RenderWidgetHostImpl::OnUpdateIsDelayed() {
  if (in_get_backing_store_)
    abort_get_backing_store_ = true;
}

void RenderWidgetHostImpl::DidUpdateBackingStore(
    const ViewHostMsg_UpdateRect_Params& params,
    const TimeTicks& paint_start) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::DidUpdateBackingStore");
  TimeTicks update_start = TimeTicks::Now();

  if (params.needs_ack) {
    // ACK early so we can prefetch the next PaintRect if there is a next one.
    // This must be done AFTER we're done painting with the bitmap supplied by
    // the renderer. This ACK is a signal to the renderer that the backing store
    // can be re-used, so the bitmap may be invalid after this call.
    Send(new ViewMsg_UpdateRect_ACK(routing_id_));
  }

  // Move the plugins if the view hasn't already been destroyed.  Plugin moves
  // will not be re-issued, so must move them now, regardless of whether we
  // paint or not.  MovePluginWindows attempts to move the plugin windows and
  // in the process could dispatch other window messages which could cause the
  // view to be destroyed.
  if (view_)
    view_->MovePluginWindows(params.scroll_offset, params.plugin_window_moves);

  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // us more data.
  if (is_hidden_)
    return;

  // Now paint the view. Watch out: it might be destroyed already.
  if (view_ && !is_accelerated_compositing_active_) {
    view_being_painted_ = true;
    view_->DidUpdateBackingStore(params.scroll_rect, params.scroll_delta,
                                 params.copy_rects, params.latency_info);
    view_->DidReceiveRendererFrame();
    view_being_painted_ = false;
  }

  // If we got a resize ack, then perhaps we have another resize to send?
  bool is_resize_ack =
      ViewHostMsg_UpdateRect_Flags::is_resize_ack(params.flags);
  if (is_resize_ack)
    WasResized();

  // Log the time delta for processing a paint message.
  TimeTicks now = TimeTicks::Now();
  TimeDelta delta = now - update_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_DidUpdateBackingStore", delta);

  // Measures the time from receiving the MsgUpdateRect IPC to completing the
  // DidUpdateBackingStore() method.  On platforms which have asynchronous
  // painting, such as Linux, this is the sum of MPArch.RWH_OnMsgUpdateRect,
  // MPArch.RWH_DidUpdateBackingStore, and the time spent asynchronously
  // waiting for the paint to complete.
  //
  // On other platforms, this will be equivalent to MPArch.RWH_OnMsgUpdateRect.
  delta = now - paint_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_TotalPaintTime", delta);
}

void RenderWidgetHostImpl::OnBeginSmoothScroll(
    const ViewHostMsg_BeginSmoothScroll_Params& params) {
  if (!view_)
    return;
  smooth_scroll_gesture_controller_.BeginSmoothScroll(view_, params);
}

void RenderWidgetHostImpl::OnFocus() {
  // Only RenderViewHost can deal with that message.
  RecordAction(UserMetricsAction("BadMessageTerminate_RWH4"));
  GetProcess()->ReceivedBadMessage();
}

void RenderWidgetHostImpl::OnBlur() {
  // Only RenderViewHost can deal with that message.
  RecordAction(UserMetricsAction("BadMessageTerminate_RWH5"));
  GetProcess()->ReceivedBadMessage();
}

void RenderWidgetHostImpl::OnSetCursor(const WebCursor& cursor) {
  if (!view_) {
    return;
  }
  view_->UpdateCursor(cursor);
}

void RenderWidgetHostImpl::OnTextInputTypeChanged(
    ui::TextInputType type,
    bool can_compose_inline,
    ui::TextInputMode input_mode) {
  if (view_)
    view_->TextInputTypeChanged(type, can_compose_inline, input_mode);
}

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
void RenderWidgetHostImpl::OnImeCompositionRangeChanged(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (view_)
    view_->ImeCompositionRangeChanged(range, character_bounds);
}
#endif

void RenderWidgetHostImpl::OnImeCancelComposition() {
  if (view_)
    view_->ImeCancelComposition();
}

void RenderWidgetHostImpl::OnDidActivateAcceleratedCompositing(bool activated) {
  TRACE_EVENT1("renderer_host",
               "RenderWidgetHostImpl::OnDidActivateAcceleratedCompositing",
               "activated", activated);
  is_accelerated_compositing_active_ = activated;
  if (view_)
    view_->OnAcceleratedCompositingStateChange();
}

void RenderWidgetHostImpl::OnLockMouse(bool user_gesture,
                                       bool last_unlocked_by_target,
                                       bool privileged) {

  if (pending_mouse_lock_request_) {
    Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
    return;
  } else if (IsMouseLocked()) {
    Send(new ViewMsg_LockMouse_ACK(routing_id_, true));
    return;
  }

  pending_mouse_lock_request_ = true;
  if (privileged && allow_privileged_mouse_lock_) {
    // Directly approve to lock the mouse.
    GotResponseToLockMouseRequest(true);
  } else {
    RequestToLockMouse(user_gesture, last_unlocked_by_target);
  }
}

void RenderWidgetHostImpl::OnUnlockMouse() {
  RejectMouseLockOrUnlockIfNecessary();
}

void RenderWidgetHostImpl::OnShowDisambiguationPopup(
    const gfx::Rect& rect,
    const gfx::Size& size,
    const TransportDIB::Id& id) {
  DCHECK(!rect.IsEmpty());
  DCHECK(!size.IsEmpty());

  TransportDIB* dib = process_->GetTransportDIB(id);
  DCHECK(dib->memory());
  DCHECK(dib->size() == SkBitmap::ComputeSize(SkBitmap::kARGB_8888_Config,
                                              size.width(), size.height()));

  SkBitmap zoomed_bitmap;
  zoomed_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
      size.width(), size.height());
  zoomed_bitmap.setPixels(dib->memory());

#if defined(OS_ANDROID)
  if (view_)
    view_->ShowDisambiguationPopup(rect, zoomed_bitmap);
#else
  NOTIMPLEMENTED();
#endif

  zoomed_bitmap.setPixels(0);
  Send(new ViewMsg_ReleaseDisambiguationPopupDIB(GetRoutingID(),
                                                 dib->handle()));
}

#if defined(OS_WIN)
void RenderWidgetHostImpl::OnWindowlessPluginDummyWindowCreated(
    gfx::NativeViewId dummy_activation_window) {
  HWND hwnd = reinterpret_cast<HWND>(dummy_activation_window);

  // This may happen as a result of a race condition when the plugin is going
  // away.
  wchar_t window_title[MAX_PATH + 1] = {0};
  if (!IsWindow(hwnd) ||
      !GetWindowText(hwnd, window_title, arraysize(window_title)) ||
      lstrcmpiW(window_title, kDummyActivationWindowName) != 0) {
    return;
  }

  SetParent(hwnd, reinterpret_cast<HWND>(GetNativeViewId()));
  dummy_windows_for_activation_.push_back(hwnd);
}

void RenderWidgetHostImpl::OnWindowlessPluginDummyWindowDestroyed(
    gfx::NativeViewId dummy_activation_window) {
  HWND hwnd = reinterpret_cast<HWND>(dummy_activation_window);
  std::list<HWND>::iterator i = dummy_windows_for_activation_.begin();
  for (; i != dummy_windows_for_activation_.end(); ++i) {
    if ((*i) == hwnd) {
      dummy_windows_for_activation_.erase(i);
      return;
    }
  }
  NOTREACHED() << "Unknown dummy window";
}
#endif

bool RenderWidgetHostImpl::PaintBackingStoreRect(
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    const gfx::Size& view_size,
    float scale_factor,
    const base::Closure& completion_callback) {
  // The view may be destroyed already.
  if (!view_)
    return false;

  if (is_hidden_) {
    // Don't bother updating the backing store when we're hidden. Just mark it
    // as being totally invalid. This will cause a complete repaint when the
    // view is restored.
    needs_repainting_on_restore_ = true;
    return false;
  }

  bool needs_full_paint = false;
  bool scheduled_completion_callback = false;
  BackingStoreManager::PrepareBackingStore(this, view_size, bitmap, bitmap_rect,
                                           copy_rects, scale_factor,
                                           completion_callback,
                                           &needs_full_paint,
                                           &scheduled_completion_callback);
  if (needs_full_paint) {
    repaint_start_time_ = TimeTicks::Now();
    DCHECK(!repaint_ack_pending_);
    repaint_ack_pending_ = true;
    TRACE_EVENT_ASYNC_BEGIN0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }

  return scheduled_completion_callback;
}

void RenderWidgetHostImpl::ScrollBackingStoreRect(const gfx::Vector2d& delta,
                                                  const gfx::Rect& clip_rect,
                                                  const gfx::Size& view_size) {
  if (is_hidden_) {
    // Don't bother updating the backing store when we're hidden. Just mark it
    // as being totally invalid. This will cause a complete repaint when the
    // view is restored.
    needs_repainting_on_restore_ = true;
    return;
  }

  // TODO(darin): do we need to do something else if our backing store is not
  // the same size as the advertised view?  maybe we just assume there is a
  // full paint on its way?
  BackingStore* backing_store = BackingStoreManager::Lookup(this);
  if (!backing_store || (backing_store->size() != view_size))
    return;
  backing_store->ScrollBackingStore(delta, clip_rect, view_size);
}

void RenderWidgetHostImpl::Replace(const string16& word) {
  Send(new InputMsg_Replace(routing_id_, word));
}

void RenderWidgetHostImpl::ReplaceMisspelling(const string16& word) {
  Send(new InputMsg_ReplaceMisspelling(routing_id_, word));
}

void RenderWidgetHostImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  ignore_input_events_ = ignore_input_events;
}

bool RenderWidgetHostImpl::KeyPressListenersHandleEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type != WebKeyboardEvent::RawKeyDown)
    return false;

  ObserverList<KeyboardListener>::Iterator it(keyboard_listeners_);
  KeyboardListener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->HandleKeyPressEvent(event))
      return true;
  }

  return false;
}

InputEventAckState RenderWidgetHostImpl::FilterInputEvent(
    const WebKit::WebInputEvent& event, const ui::LatencyInfo& latency_info) {
  if (overscroll_controller() &&
      !overscroll_controller()->WillDispatchEvent(event, latency_info)) {
    return INPUT_EVENT_ACK_STATE_UNKNOWN;
  }

  return view_ ? view_->FilterInputEvent(event)
               : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostImpl::IncrementInFlightEventCount() {
  StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(hung_renderer_delay_ms_));
  increment_in_flight_event_count();
}

void RenderWidgetHostImpl::DecrementInFlightEventCount() {
  DCHECK(in_flight_event_count_ >= 0);
  // Cancel pending hung renderer checks since the renderer is responsive.
  if (decrement_in_flight_event_count() <= 0)
    StopHangMonitorTimeout();
}

void RenderWidgetHostImpl::OnHasTouchEventHandlers(bool has_handlers) {
  if (has_touch_handler_ == has_handlers)
    return;
  has_touch_handler_ = has_handlers;
#if defined(OS_ANDROID)
  if (view_)
    view_->HasTouchEventHandlers(has_touch_handler_);
#endif
}

bool RenderWidgetHostImpl::OnSendKeyboardEvent(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency_info,
    bool* is_shortcut) {
  if (IgnoreInputEvents())
    return false;

  if (!process_->HasConnection())
    return false;

  // First, let keypress listeners take a shot at handling the event.  If a
  // listener handles the event, it should not be propagated to the renderer.
  if (KeyPressListenersHandleEvent(key_event)) {
    // Some keypresses that are accepted by the listener might have follow up
    // char events, which should be ignored.
    if (key_event.type == WebKeyboardEvent::RawKeyDown)
      suppress_next_char_events_ = true;
    return false;
  }

  if (key_event.type == WebKeyboardEvent::Char &&
      (key_event.windowsKeyCode == ui::VKEY_RETURN ||
       key_event.windowsKeyCode == ui::VKEY_SPACE)) {
    OnUserGesture();
  }

  // Double check the type to make sure caller hasn't sent us nonsense that
  // will mess up our key queue.
  if (!WebInputEvent::isKeyboardEventType(key_event.type))
    return false;

  if (suppress_next_char_events_) {
    // If preceding RawKeyDown event was handled by the browser, then we need
    // suppress all Char events generated by it. Please note that, one
    // RawKeyDown event may generate multiple Char events, so we can't reset
    // |suppress_next_char_events_| until we get a KeyUp or a RawKeyDown.
    if (key_event.type == WebKeyboardEvent::Char)
      return false;
    // We get a KeyUp or a RawKeyDown event.
    suppress_next_char_events_ = false;
  }

  // Only pre-handle the key event if it's not handled by the input method.
  if (delegate_ && !key_event.skip_in_browser) {
    // We need to set |suppress_next_char_events_| to true if
    // PreHandleKeyboardEvent() returns true, but |this| may already be
    // destroyed at that time. So set |suppress_next_char_events_| true here,
    // then revert it afterwards when necessary.
    if (key_event.type == WebKeyboardEvent::RawKeyDown)
      suppress_next_char_events_ = true;

    // Tab switching/closing accelerators aren't sent to the renderer to avoid
    // a hung/malicious renderer from interfering.
    if (delegate_->PreHandleKeyboardEvent(key_event, is_shortcut))
      return false;

    if (key_event.type == WebKeyboardEvent::RawKeyDown)
      suppress_next_char_events_ = false;
  }

  return true;
}

bool RenderWidgetHostImpl::OnSendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  if (IgnoreInputEvents())
    return false;

  if (delegate_->PreHandleWheelEvent(wheel_event.event))
    return false;

  return true;
}

bool RenderWidgetHostImpl::OnSendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (IgnoreInputEvents())
    return false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateTouchScreenWithMouse)) {
    SimulateTouchGestureWithMouse(mouse_event.event);
    return false;
  }

  return true;
}

bool RenderWidgetHostImpl::OnSendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  return !IgnoreInputEvents();
}

bool RenderWidgetHostImpl::OnSendGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (IgnoreInputEvents())
    return false;

  if (!IsInOverscrollGesture() &&
      !input_router_->ShouldForwardGestureEvent(gesture_event)) {
    if (overscroll_controller_.get())
      overscroll_controller_->DiscardingGestureEvent(gesture_event.event);
    return false;
  }

  return true;
}

bool RenderWidgetHostImpl::OnSendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) {
  TRACE_EVENT_INSTANT0("input",
                       "RenderWidgetHostImpl::OnSendMouseEventImmediately",
                       TRACE_EVENT_SCOPE_THREAD);
  if (IgnoreInputEvents())
    return false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateTouchScreenWithMouse)) {
    SimulateTouchGestureWithMouse(mouse_event.event);
    return false;
  }

  if (mouse_event.event.type == WebInputEvent::MouseDown)
    OnUserGesture();

  return true;
}

bool RenderWidgetHostImpl::OnSendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) {
  TRACE_EVENT_INSTANT0("input",
                       "RenderWidgetHostImpl::OnSendTouchEventImmediately",
                       TRACE_EVENT_SCOPE_THREAD);
  return !IgnoreInputEvents();
}

bool RenderWidgetHostImpl::OnSendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) {
  TRACE_EVENT_INSTANT0("input",
                       "RenderWidgetHostImpl::OnSendGestureEventImmediately",
                       TRACE_EVENT_SCOPE_THREAD);
  return !IgnoreInputEvents();
}

void RenderWidgetHostImpl::OnKeyboardEventAck(
      const NativeWebKeyboardEvent& event,
      InputEventAckState ack_result) {
#if defined(OS_MACOSX)
  if (!is_hidden() && view_ && view_->PostProcessEventForPluginIme(event))
    return;
#endif

  // We only send unprocessed key event upwards if we are not hidden,
  // because the user has moved away from us and no longer expect any effect
  // of this key event.
  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (delegate_ && !processed && !is_hidden() && !event.skip_in_browser) {
    delegate_->HandleKeyboardEvent(event);

    // WARNING: This RenderWidgetHostImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this RenderWidgetHostImpl).
  }
}

void RenderWidgetHostImpl::OnWheelEventAck(
    const WebKit::WebMouseWheelEvent& wheel_event,
    InputEventAckState ack_result) {
  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (overscroll_controller_)
    overscroll_controller_->ReceivedEventACK(wheel_event, processed);

  if (!processed && !is_hidden() && view_)
    view_->UnhandledWheelEvent(wheel_event);
}

void RenderWidgetHostImpl::OnGestureEventAck(
    const WebKit::WebGestureEvent& event,
    InputEventAckState ack_result) {
  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (overscroll_controller_)
    overscroll_controller_->ReceivedEventACK(event, processed);

  if (view_)
    view_->GestureEventAck(event.type, ack_result);
}

void RenderWidgetHostImpl::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  ComputeTouchLatency(event.latency);
  if (view_)
    view_->ProcessAckedTouchEvent(event, ack_result);
}

void RenderWidgetHostImpl::OnUnexpectedEventAck(bool bad_message) {
  if (bad_message) {
    RecordAction(UserMetricsAction("BadMessageTerminate_RWH2"));
    process_->ReceivedBadMessage();
  }

  suppress_next_char_events_ = false;
}

const gfx::Vector2d& RenderWidgetHostImpl::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

bool RenderWidgetHostImpl::IgnoreInputEvents() const {
  return ignore_input_events_ || process_->IgnoreInputEvents();
}

bool RenderWidgetHostImpl::ShouldForwardTouchEvent() const {
  return input_router_->ShouldForwardTouchEvent();
}

bool RenderWidgetHostImpl::ShouldForwardGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return input_router_->ShouldForwardGestureEvent(gesture_event);
}

bool RenderWidgetHostImpl::HasQueuedGestureEvents() const {
  return input_router_->HasQueuedGestureEvents();
}

void RenderWidgetHostImpl::StartUserGesture() {
  OnUserGesture();
}

void RenderWidgetHostImpl::Stop() {
  Send(new ViewMsg_Stop(GetRoutingID()));
}

void RenderWidgetHostImpl::SetBackground(const SkBitmap& background) {
  Send(new ViewMsg_SetBackground(GetRoutingID(), background));
}

void RenderWidgetHostImpl::SetEditCommandsForNextKeyEvent(
    const std::vector<EditCommand>& commands) {
  Send(new InputMsg_SetEditCommandsForNextKeyEvent(GetRoutingID(), commands));
}

void RenderWidgetHostImpl::SetAccessibilityMode(AccessibilityMode mode) {
  accessibility_mode_ = mode;
  Send(new ViewMsg_SetAccessibilityMode(GetRoutingID(), mode));
}

void RenderWidgetHostImpl::AccessibilityDoDefaultAction(int object_id) {
  Send(new AccessibilityMsg_DoDefaultAction(GetRoutingID(), object_id));
}

void RenderWidgetHostImpl::AccessibilitySetFocus(int object_id) {
  Send(new AccessibilityMsg_SetFocus(GetRoutingID(), object_id));
}

void RenderWidgetHostImpl::AccessibilityScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  Send(new AccessibilityMsg_ScrollToMakeVisible(
      GetRoutingID(), acc_obj_id, subfocus));
}

void RenderWidgetHostImpl::AccessibilityScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  Send(new AccessibilityMsg_ScrollToPoint(
      GetRoutingID(), acc_obj_id, point));
}

void RenderWidgetHostImpl::AccessibilitySetTextSelection(
    int object_id, int start_offset, int end_offset) {
  Send(new AccessibilityMsg_SetTextSelection(
      GetRoutingID(), object_id, start_offset, end_offset));
}

void RenderWidgetHostImpl::FatalAccessibilityTreeError() {
  Send(new AccessibilityMsg_FatalError(GetRoutingID()));
}

#if defined(OS_WIN) && defined(USE_AURA)
void RenderWidgetHostImpl::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
  if (view_)
    view_->SetParentNativeViewAccessible(accessible_parent);
}

gfx::NativeViewAccessible
RenderWidgetHostImpl::GetParentNativeViewAccessible() const {
  return delegate_->GetParentNativeViewAccessible();
}
#endif

void RenderWidgetHostImpl::ExecuteEditCommand(const std::string& command,
                                              const std::string& value) {
  Send(new InputMsg_ExecuteEditCommand(GetRoutingID(), command, value));
}

void RenderWidgetHostImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  Send(new InputMsg_ScrollFocusedEditableNodeIntoRect(GetRoutingID(), rect));
}

void RenderWidgetHostImpl::SelectRange(const gfx::Point& start,
                                       const gfx::Point& end) {
  Send(new InputMsg_SelectRange(GetRoutingID(), start, end));
}

void RenderWidgetHostImpl::MoveCaret(const gfx::Point& point) {
  Send(new InputMsg_MoveCaret(GetRoutingID(), point));
}

void RenderWidgetHostImpl::Undo() {
  Send(new InputMsg_Undo(GetRoutingID()));
  RecordAction(UserMetricsAction("Undo"));
}

void RenderWidgetHostImpl::Redo() {
  Send(new InputMsg_Redo(GetRoutingID()));
  RecordAction(UserMetricsAction("Redo"));
}

void RenderWidgetHostImpl::Cut() {
  Send(new InputMsg_Cut(GetRoutingID()));
  RecordAction(UserMetricsAction("Cut"));
}

void RenderWidgetHostImpl::Copy() {
  Send(new InputMsg_Copy(GetRoutingID()));
  RecordAction(UserMetricsAction("Copy"));
}

void RenderWidgetHostImpl::CopyToFindPboard() {
#if defined(OS_MACOSX)
  // Windows/Linux don't have the concept of a find pasteboard.
  Send(new InputMsg_CopyToFindPboard(GetRoutingID()));
  RecordAction(UserMetricsAction("CopyToFindPboard"));
#endif
}

void RenderWidgetHostImpl::Paste() {
  Send(new InputMsg_Paste(GetRoutingID()));
  RecordAction(UserMetricsAction("Paste"));
}

void RenderWidgetHostImpl::PasteAndMatchStyle() {
  Send(new InputMsg_PasteAndMatchStyle(GetRoutingID()));
  RecordAction(UserMetricsAction("PasteAndMatchStyle"));
}

void RenderWidgetHostImpl::Delete() {
  Send(new InputMsg_Delete(GetRoutingID()));
  RecordAction(UserMetricsAction("DeleteSelection"));
}

void RenderWidgetHostImpl::SelectAll() {
  Send(new InputMsg_SelectAll(GetRoutingID()));
  RecordAction(UserMetricsAction("SelectAll"));
}

void RenderWidgetHostImpl::Unselect() {
  Send(new InputMsg_Unselect(GetRoutingID()));
  RecordAction(UserMetricsAction("Unselect"));
}

bool RenderWidgetHostImpl::GotResponseToLockMouseRequest(bool allowed) {
  if (!allowed) {
    RejectMouseLockOrUnlockIfNecessary();
    return false;
  } else {
    if (!pending_mouse_lock_request_) {
      // This is possible, e.g., the plugin sends us an unlock request before
      // the user allows to lock to mouse.
      return false;
    }

    pending_mouse_lock_request_ = false;
    if (!view_ || !view_->HasFocus()|| !view_->LockMouse()) {
      Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
      return false;
    } else {
      Send(new ViewMsg_LockMouse_ACK(routing_id_, true));
      return true;
    }
  }
}

// static
void RenderWidgetHostImpl::AcknowledgeBufferPresent(
    int32 route_id, int gpu_host_id,
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(gpu_host_id);
  if (ui_shim) {
    ui_shim->Send(new AcceleratedSurfaceMsg_BufferPresented(route_id,
                                                            params));
  }
}

// static
void RenderWidgetHostImpl::SendSwapCompositorFrameAck(
    int32 route_id,
    uint32 output_surface_id,
    int renderer_host_id,
    const cc::CompositorFrameAck& ack) {
  RenderProcessHost* host = RenderProcessHost::FromID(renderer_host_id);
  if (!host)
    return;
  host->Send(new ViewMsg_SwapCompositorFrameAck(
      route_id, output_surface_id, ack));
}

void RenderWidgetHostImpl::AcknowledgeSwapBuffersToRenderer() {
  if (!is_threaded_compositing_enabled_)
    Send(new ViewMsg_SwapBuffers_ACK(routing_id_));
}

#if defined(USE_AURA)

void RenderWidgetHostImpl::ParentChanged(gfx::NativeViewId new_parent) {
#if defined(OS_WIN)
  HWND hwnd = reinterpret_cast<HWND>(new_parent);
  if (!hwnd)
    hwnd = GetDesktopWindow();
  for (std::list<HWND>::iterator i = dummy_windows_for_activation_.begin();
        i != dummy_windows_for_activation_.end(); ++i) {
    SetParent(*i, hwnd);
  }
#endif
}

#endif

void RenderWidgetHostImpl::DelayedAutoResized() {
  gfx::Size new_size = new_auto_size_;
  // Clear the new_auto_size_ since the empty value is used as a flag to
  // indicate that no callback is in progress (i.e. without this line
  // DelayedAutoResized will not get called again).
  new_auto_size_.SetSize(0, 0);
  if (!should_auto_resize_)
    return;

  OnRenderAutoResized(new_size);
}

void RenderWidgetHostImpl::DetachDelegate() {
  delegate_ = NULL;
}

void RenderWidgetHostImpl::ComputeTouchLatency(
    const ui::LatencyInfo& latency_info) {
  ui::LatencyInfo::LatencyComponent ui_component;
  ui::LatencyInfo::LatencyComponent rwh_component;
  ui::LatencyInfo::LatencyComponent acked_component;

  if (!latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_UI_COMPONENT,
                                0,
                                &ui_component) ||
      !latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_RWH_COMPONENT,
                                GetLatencyComponentId(),
                                &rwh_component))
    return;

  DCHECK(ui_component.event_count == 1);
  DCHECK(rwh_component.event_count == 1);

  base::TimeDelta ui_delta =
      rwh_component.event_time - ui_component.event_time;
  rendering_stats_.touch_ui_count++;
  rendering_stats_.total_touch_ui_latency += ui_delta;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Event.Latency.Browser.TouchUI",
      ui_delta.InMicroseconds(),
      0,
      20000,
      100);

  if (latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_ACKED_COMPONENT,
                               0,
                               &acked_component)) {
    DCHECK(acked_component.event_count == 1);
    base::TimeDelta acked_delta =
        acked_component.event_time - rwh_component.event_time;
    rendering_stats_.touch_acked_count++;
    rendering_stats_.total_touch_acked_latency += acked_delta;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Latency.Browser.TouchAcked",
        acked_delta.InMicroseconds(),
        0,
        1000000,
        100);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuBenchmarking))
    Send(new ViewMsg_SetBrowserRenderingStats(routing_id_, rendering_stats_));
}

void RenderWidgetHostImpl::FrameSwapped(const ui::LatencyInfo& latency_info) {
  ui::LatencyInfo::LatencyComponent rwh_component;
  if (!latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_RWH_COMPONENT,
                                GetLatencyComponentId(),
                                &rwh_component))
    return;

  rendering_stats_.input_event_count += rwh_component.event_count;
  rendering_stats_.total_input_latency +=
      rwh_component.event_count *
      (latency_info.swap_timestamp - rwh_component.event_time);

  ui::LatencyInfo::LatencyComponent original_component;
  if (latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          GetLatencyComponentId(),
          &original_component)) {
    // This UMA metric tracks the time from when the original touch event is
    // created (averaged if there are multiple) to when the scroll gesture
    // results in final frame swap.
    base::TimeDelta delta =
        latency_info.swap_timestamp - original_component.event_time;
    for (size_t i = 0; i < original_component.event_count; i++) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.TouchToScrollUpdateSwap",
          delta.InMicroseconds(),
          0,
          1000000,
          100);
    }
    rendering_stats_.scroll_update_count += original_component.event_count;
    rendering_stats_.total_scroll_update_latency +=
        original_component.event_count *
        (latency_info.swap_timestamp - original_component.event_time);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuBenchmarking))
    Send(new ViewMsg_SetBrowserRenderingStats(routing_id_, rendering_stats_));
}

void RenderWidgetHostImpl::DidReceiveRendererFrame() {
  view_->DidReceiveRendererFrame();
}

// static
void RenderWidgetHostImpl::CompositorFrameDrawn(
    const ui::LatencyInfo& latency_info) {
  for (ui::LatencyInfo::LatencyMap::const_iterator b =
           latency_info.latency_components.begin();
       b != latency_info.latency_components.end();
       ++b) {
    if (b->first.first != ui::INPUT_EVENT_LATENCY_RWH_COMPONENT)
      continue;
    // Matches with GetLatencyComponentId
    int routing_id = b->first.second & 0xffffffff;
    int process_id = (b->first.second >> 32) & 0xffffffff;
    RenderWidgetHost* rwh =
        RenderWidgetHost::FromID(process_id, routing_id);
    if (!rwh)
      continue;
    RenderWidgetHostImpl::From(rwh)->FrameSwapped(latency_info);
  }
}

}  // namespace content
