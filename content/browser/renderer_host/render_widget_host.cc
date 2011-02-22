// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/accessibility/browser_accessibility_state.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/webplugin.h"

#if defined(TOOLKIT_VIEWS)
#include "views/view.h"
#endif

#if defined (OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTextDirection;

#if defined (OS_MACOSX)
using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;
#endif

// How long to (synchronously) wait for the renderer to respond with a
// PaintRect message, when our backing-store is invalid, before giving up and
// returning a null or incorrectly sized backing-store from GetBackingStore.
// This timeout impacts the "choppiness" of our window resize perf.
static const int kPaintMsgTimeoutMS = 40;

// How long to wait before we consider a renderer hung.
static const int kHungRendererDelayMs = 20000;

// The maximum time between wheel messages while coalescing. This trades off
// smoothness of scrolling with a risk of falling behind the events, resulting
// in trailing scrolls after the user ends their input.
static const int kMaxTimeBetweenWheelMessagesMs = 250;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost

RenderWidgetHost::RenderWidgetHost(RenderProcessHost* process,
                                   int routing_id)
    : renderer_initialized_(false),
      renderer_accessible_(false),
      view_(NULL),
      process_(process),
      routing_id_(routing_id),
      is_loading_(false),
      is_hidden_(false),
      is_accelerated_compositing_active_(false),
      repaint_ack_pending_(false),
      resize_ack_pending_(false),
      mouse_move_pending_(false),
      mouse_wheel_pending_(false),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      in_get_backing_store_(false),
      view_being_painted_(false),
      ignore_input_events_(false),
      text_direction_updated_(false),
      text_direction_(WebKit::WebTextDirectionLeftToRight),
      text_direction_canceled_(false),
      suppress_next_char_events_(false) {
  if (routing_id_ == MSG_ROUTING_NONE)
    routing_id_ = process_->GetNextRoutingID();

  process_->Attach(this, routing_id_);
  // Because the widget initializes as is_hidden_ == false,
  // tell the process host that we're alive.
  process_->WidgetRestored();

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility) ||
          BrowserAccessibilityState::GetInstance()->IsAccessibleBrowser()) {
    EnableRendererAccessibility();
  }
}

RenderWidgetHost::~RenderWidgetHost() {
  // Clear our current or cached backing store if either remains.
  BackingStoreManager::RemoveBackingStore(this);

  process_->Release(routing_id_);
}

gfx::NativeViewId RenderWidgetHost::GetNativeViewId() {
  if (view_)
    return gfx::IdFromNativeView(view_->GetNativeView());
  return 0;
}

bool RenderWidgetHost::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

void RenderWidgetHost::Init() {
  DCHECK(process_->HasConnection());

  renderer_initialized_ = true;

  // Send the ack along with the information on placement.
  Send(new ViewMsg_CreatingNew_ACK(routing_id_, GetNativeViewId()));
  WasResized();
}

void RenderWidgetHost::Shutdown() {
  if (process_->HasConnection()) {
    // Tell the renderer object to close.
    process_->ReportExpectingClose(routing_id_);
    bool rv = Send(new ViewMsg_Close(routing_id_));
    DCHECK(rv);
  }

  Destroy();
}

bool RenderWidgetHost::IsRenderView() const {
  return false;
}

bool RenderWidgetHost::OnMessageReceived(const IPC::Message &msg) {
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderWidgetHost, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnMsgRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnMsgRenderViewGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnMsgClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnMsgRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PaintAtSize_ACK, OnMsgPaintAtSizeAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnMsgUpdateRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnMsgInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnMsgFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Blur, OnMsgBlur)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnMsgSetCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeUpdateTextInputState,
                        OnMsgImeUpdateTextInputState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCancelComposition,
                        OnMsgImeCancelComposition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidActivateAcceleratedCompositing,
                        OnMsgDidActivateAcceleratedCompositing)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetScreenInfo, OnMsgGetScreenInfo)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetWindowRect, OnMsgGetWindowRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetRootWindowRect, OnMsgGetRootWindowRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginFocusChanged,
                        OnMsgPluginFocusChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartPluginIme,
                        OnMsgStartPluginIme)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateFakePluginWindowHandle,
                        OnAllocateFakePluginWindowHandle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DestroyFakePluginWindowHandle,
                        OnDestroyFakePluginWindowHandle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AcceleratedSurfaceSetIOSurface,
                        OnAcceleratedSurfaceSetIOSurface)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AcceleratedSurfaceSetTransportDIB,
                        OnAcceleratedSurfaceSetTransportDIB)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#elif defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreatePluginContainer,
                        OnMsgCreatePluginContainer)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DestroyPluginContainer,
                        OnMsgDestroyPluginContainer)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message de-serialization failed. Kill the renderer process.
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RWH"));
    process()->ReceivedBadMessage();
  }
  return handled;
}

bool RenderWidgetHost::Send(IPC::Message* msg) {
  return process_->Send(msg);
}

void RenderWidgetHost::WasHidden() {
  is_hidden_ = true;

  // Don't bother reporting hung state when we aren't the active tab.
  StopHangMonitorTimeout();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  Send(new ViewMsg_WasHidden(routing_id_));

  // TODO(darin): what about constrained windows?  it doesn't look like they
  // see a message when their parent is hidden.  maybe there is something more
  // generic we can do at the TabContents API level instead of relying on
  // Windows messages.

  // Tell the RenderProcessHost we were hidden.
  process_->WidgetHidden();

  bool is_visible = false;
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));
}

void RenderWidgetHost::WasRestored() {
  // When we create the widget, it is created as *not* hidden.
  if (!is_hidden_)
    return;
  is_hidden_ = false;

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
  Send(new ViewMsg_WasRestored(routing_id_, needs_repainting));

  process_->WidgetRestored();

  bool is_visible = true;
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));

  // It's possible for our size to be out of sync with the renderer. The
  // following is one case that leads to this:
  // 1. WasResized -> Send ViewMsg_Resize to render
  // 2. WasResized -> do nothing as resize_ack_pending_ is true
  // 3. WasHidden
  // 4. OnMsgUpdateRect from (1) processed. Does NOT invoke WasResized as view
  //    is hidden. Now renderer/browser out of sync with what they think size
  //    is.
  // By invoking WasResized the renderer is updated as necessary. WasResized
  // does nothing if the sizes are already in sync.
  //
  // TODO: ideally ViewMsg_WasRestored would take a size. This way, the renderer
  // could handle both the restore and resize at once. This isn't that big a
  // deal as RenderWidget::WasRestored delays updating, so that the resize from
  // WasResized is usually processed before the renderer is painted.
  WasResized();
}

void RenderWidgetHost::WasResized() {
  if (resize_ack_pending_ || !process_->HasConnection() || !view_ ||
      !renderer_initialized_) {
    return;
  }

#if !defined(OS_MACOSX)
  gfx::Size new_size = view_->GetViewBounds().size();
#else
  // When UI scaling is enabled on OS X, allocate a smaller bitmap and
  // pixel-scale it up.
  // TODO(thakis): Use pixel size on mac and set UI scale in renderer.
  // http://crbug.com/31960
  gfx::Size new_size(view_->GetViewCocoaBounds().size());
#endif
  gfx::Rect reserved_rect = view_->reserved_contents_rect();

  // Avoid asking the RenderWidget to resize to its current size, since it
  // won't send us a PaintRect message in that case, unless reserved area is
  // changed, but even in this case PaintRect message won't be sent.
  if (new_size == current_size_ && reserved_rect == current_reserved_rect_)
    return;

  if (in_flight_size_ != gfx::Size() && new_size == in_flight_size_ &&
      in_flight_reserved_rect_ == reserved_rect) {
    return;
  }

  // We don't expect to receive an ACK when the requested size is empty or
  // only reserved area is changed.
  resize_ack_pending_ = !new_size.IsEmpty() && new_size != current_size_;

  if (!Send(new ViewMsg_Resize(routing_id_, new_size, reserved_rect))) {
    resize_ack_pending_ = false;
  } else {
    if (resize_ack_pending_) {
      in_flight_size_ = new_size;
      in_flight_reserved_rect_ = reserved_rect;
    } else {
      // Message was sent successfully, but we do not expect to receive an ACK,
      // so update current values right away.
      current_size_ = new_size;
      // TODO(alekseys): send a message from renderer to ack a reserved rect
      // changes only.
      current_reserved_rect_ = reserved_rect;
    }
  }
}

void RenderWidgetHost::GotFocus() {
  Focus();
}

void RenderWidgetHost::Focus() {
  Send(new ViewMsg_SetFocus(routing_id_, true));
}

void RenderWidgetHost::Blur() {
  Send(new ViewMsg_SetFocus(routing_id_, false));
}

void RenderWidgetHost::LostCapture() {
  Send(new ViewMsg_MouseCaptureLost(routing_id_));
}

void RenderWidgetHost::ViewDestroyed() {
  // TODO(evanm): tracking this may no longer be necessary;
  // eliminate this function if so.
  view_ = NULL;
}

void RenderWidgetHost::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  if (!view_)
    return;
  view_->SetIsLoading(is_loading);
}

void RenderWidgetHost::PaintAtSize(TransportDIB::Handle dib_handle,
                                   int tag,
                                   const gfx::Size& page_size,
                                   const gfx::Size& desired_size) {
  // Ask the renderer to create a bitmap regardless of whether it's
  // hidden, being resized, redrawn, etc.  It resizes the web widget
  // to the page_size and then scales it to the desired_size.
  Send(new ViewMsg_PaintAtSize(routing_id_, dib_handle, tag,
                               page_size, desired_size));
}

BackingStore* RenderWidgetHost::GetBackingStore(bool force_create) {
  // We should not be asked to paint while we are hidden.  If we are hidden,
  // then it means that our consumer failed to call WasRestored. If we're not
  // force creating the backing store, it's OK since we can feel free to give
  // out our cached one if we have it.
  DCHECK(!is_hidden_ || !force_create) <<
      "GetBackingStore called while hidden!";

  // We should never be called recursively; this can theoretically lead to
  // infinite recursion and almost certainly leads to lower performance.
  DCHECK(!in_get_backing_store_) << "GetBackingStore called recursively!";
  AutoReset<bool> auto_reset_in_get_backing_store(&in_get_backing_store_, true);

  // We might have a cached backing store that we can reuse!
  BackingStore* backing_store =
      BackingStoreManager::GetBackingStore(this, current_size_);
  if (!force_create)
    return backing_store;

  // If we fail to find a backing store in the cache, send out a request
  // to the renderer to paint the view if required.
  if (!backing_store && !repaint_ack_pending_ && !resize_ack_pending_ &&
      !view_being_painted_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    Send(new ViewMsg_Repaint(routing_id_, current_size_));
  }

  // When we have asked the RenderWidget to resize, and we are still waiting on
  // a response, block for a little while to see if we can't get a response
  // before returning the old (incorrectly sized) backing store.
  if (resize_ack_pending_ || !backing_store) {
    IPC::Message msg;
    TimeDelta max_delay = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
    if (process_->WaitForUpdateMsg(routing_id_, max_delay, &msg)) {
      OnMessageReceived(msg);
      backing_store = BackingStoreManager::GetBackingStore(this, current_size_);
    }
  }

  return backing_store;
}

BackingStore* RenderWidgetHost::AllocBackingStore(const gfx::Size& size) {
  if (!view_)
    return NULL;
  return view_->AllocBackingStore(size);
}

void RenderWidgetHost::DonePaintingToBackingStore() {
  Send(new ViewMsg_UpdateRect_ACK(routing_id()));
}

void RenderWidgetHost::ScheduleComposite() {
  if (is_hidden_ || !is_accelerated_compositing_active_) {
      return;
  }

  // Send out a request to the renderer to paint the view if required.
  if (!repaint_ack_pending_ && !resize_ack_pending_ && !view_being_painted_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    Send(new ViewMsg_Repaint(routing_id_, current_size_));
  }

  // When we have asked the RenderWidget to resize, and we are still waiting on
  // a response, block for a little while to see if we can't get a response.
  // We always block on response because we do not have a backing store.
  IPC::Message msg;
  TimeDelta max_delay = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
  if (process_->WaitForUpdateMsg(routing_id_, max_delay, &msg))
    OnMessageReceived(msg);
}

void RenderWidgetHost::StartHangMonitorTimeout(TimeDelta delay) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    return;
  }

  // If we already have a timer that will expire at or before the given delay,
  // then we have nothing more to do now.  If we have set our end time to null
  // by calling StopHangMonitorTimeout, though, we will need to restart the
  // timer.
  if (hung_renderer_timer_.IsRunning() &&
      hung_renderer_timer_.GetCurrentDelay() <= delay &&
      !time_when_considered_hung_.is_null()) {
    return;
  }

  // Either the timer is not yet running, or we need to adjust the timer to
  // fire sooner.
  time_when_considered_hung_ = Time::Now() + delay;
  hung_renderer_timer_.Stop();
  hung_renderer_timer_.Start(delay, this,
      &RenderWidgetHost::CheckRendererIsUnresponsive);
}

void RenderWidgetHost::RestartHangMonitorTimeout() {
  // Setting to null will cause StartHangMonitorTimeout to restart the timer.
  time_when_considered_hung_ = Time();
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void RenderWidgetHost::StopHangMonitorTimeout() {
  time_when_considered_hung_ = Time();
  RendererIsResponsive();

  // We do not bother to stop the hung_renderer_timer_ here in case it will be
  // started again shortly, which happens to be the common use case.
}

void RenderWidgetHost::SystemThemeChanged() {
  Send(new ViewMsg_ThemeChanged(routing_id_));
}

void RenderWidgetHost::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  if (ignore_input_events_ || process_->ignore_input_events())
    return;

  // Avoid spamming the renderer with mouse move events.  It is important
  // to note that WM_MOUSEMOVE events are anyways synthetic, but since our
  // thread is able to rapidly consume WM_MOUSEMOVE events, we may get way
  // more WM_MOUSEMOVE events than we wish to send to the renderer.
  if (mouse_event.type == WebInputEvent::MouseMove) {
    if (mouse_move_pending_) {
      next_mouse_move_.reset(new WebMouseEvent(mouse_event));
      return;
    }
    mouse_move_pending_ = true;
  } else if (mouse_event.type == WebInputEvent::MouseDown) {
    OnUserGesture();
  }

  ForwardInputEvent(mouse_event, sizeof(WebMouseEvent), false);
}

void RenderWidgetHost::OnMouseActivate() {
}

void RenderWidgetHost::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  if (ignore_input_events_ || process_->ignore_input_events())
    return;

  // If there's already a mouse wheel event waiting to be sent to the renderer,
  // add the new deltas to that event. Not doing so (e.g., by dropping the old
  // event, as for mouse moves) results in very slow scrolling on the Mac (on
  // which many, very small wheel events are sent).
  if (mouse_wheel_pending_) {
    if (coalesced_mouse_wheel_events_.empty() ||
        coalesced_mouse_wheel_events_.back().modifiers
            != wheel_event.modifiers ||
        coalesced_mouse_wheel_events_.back().scrollByPage
            != wheel_event.scrollByPage) {
      coalesced_mouse_wheel_events_.push_back(wheel_event);
    } else {
      WebMouseWheelEvent* last_wheel_event =
          &coalesced_mouse_wheel_events_.back();
      last_wheel_event->deltaX += wheel_event.deltaX;
      last_wheel_event->deltaY += wheel_event.deltaY;
      DCHECK_GE(wheel_event.timeStampSeconds,
                last_wheel_event->timeStampSeconds);
      last_wheel_event->timeStampSeconds = wheel_event.timeStampSeconds;
    }
    return;
  }
  mouse_wheel_pending_ = true;

  HISTOGRAM_COUNTS_100("MPArch.RWH_WheelQueueSize",
                       coalesced_mouse_wheel_events_.size());

  ForwardInputEvent(wheel_event, sizeof(WebMouseWheelEvent), false);
}

void RenderWidgetHost::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (ignore_input_events_ || process_->ignore_input_events())
    return;

  if (key_event.type == WebKeyboardEvent::Char &&
      (key_event.windowsKeyCode == ui::VKEY_RETURN ||
       key_event.windowsKeyCode == ui::VKEY_SPACE)) {
    OnUserGesture();
  }

  // Double check the type to make sure caller hasn't sent us nonsense that
  // will mess up our key queue.
  if (WebInputEvent::isKeyboardEventType(key_event.type)) {
    if (suppress_next_char_events_) {
      // If preceding RawKeyDown event was handled by the browser, then we need
      // suppress all Char events generated by it. Please note that, one
      // RawKeyDown event may generate multiple Char events, so we can't reset
      // |suppress_next_char_events_| until we get a KeyUp or a RawKeyDown.
      if (key_event.type == WebKeyboardEvent::Char)
        return;
      // We get a KeyUp or a RawKeyDown event.
      suppress_next_char_events_ = false;
    }

    bool is_keyboard_shortcut = false;
    // Only pre-handle the key event if it's not handled by the input method.
    if (!key_event.skip_in_browser) {
      // We need to set |suppress_next_char_events_| to true if
      // PreHandleKeyboardEvent() returns true, but |this| may already be
      // destroyed at that time. So set |suppress_next_char_events_| true here,
      // then revert it afterwards when necessary.
      if (key_event.type == WebKeyboardEvent::RawKeyDown)
        suppress_next_char_events_ = true;

      // Tab switching/closing accelerators aren't sent to the renderer to avoid
      // a hung/malicious renderer from interfering.
      if (PreHandleKeyboardEvent(key_event, &is_keyboard_shortcut))
        return;

      if (key_event.type == WebKeyboardEvent::RawKeyDown)
        suppress_next_char_events_ = false;
    }

    // Don't add this key to the queue if we have no way to send the message...
    if (!process_->HasConnection())
      return;

    // Put all WebKeyboardEvent objects in a queue since we can't trust the
    // renderer and we need to give something to the UnhandledInputEvent
    // handler.
    key_queue_.push_back(key_event);
    HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

    // Only forward the non-native portions of our event.
    ForwardInputEvent(key_event, sizeof(WebKeyboardEvent),
                      is_keyboard_shortcut);
  }
}

void RenderWidgetHost::ForwardInputEvent(const WebInputEvent& input_event,
                                         int event_size,
                                         bool is_keyboard_shortcut) {
  if (!process_->HasConnection())
    return;

  DCHECK(!process_->ignore_input_events());

  IPC::Message* message = new ViewMsg_HandleInputEvent(routing_id_);
  message->WriteData(
      reinterpret_cast<const char*>(&input_event), event_size);
  // |is_keyboard_shortcut| only makes sense for RawKeyDown events.
  if (input_event.type == WebInputEvent::RawKeyDown)
    message->WriteBool(is_keyboard_shortcut);
  input_event_start_time_ = TimeTicks::Now();
  Send(message);

  // Any non-wheel input event cancels pending wheel events.
  if (input_event.type != WebInputEvent::MouseWheel)
    coalesced_mouse_wheel_events_.clear();

  // Any input event cancels a pending mouse move event. Note that
  // |next_mouse_move_| possibly owns |input_event|, so don't use |input_event|
  // after this line.
  next_mouse_move_.reset();

  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void RenderWidgetHost::ForwardEditCommand(const std::string& name,
      const std::string& value) {
  // We don't need an implementation of this function here since the
  // only place we use this is for the case of dropdown menus and other
  // edge cases for which edit commands don't make sense.
}

void RenderWidgetHost::ForwardEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  // We don't need an implementation of this function here since this message is
  // only handled by RenderView.
}

#if defined(TOUCH_UI)
void RenderWidgetHost::ForwardTouchEvent(
    const WebKit::WebTouchEvent& touch_event) {
  ForwardInputEvent(touch_event, sizeof(WebKit::WebTouchEvent), false);
}
#endif

void RenderWidgetHost::RendererExited(base::TerminationStatus status,
                                      int exit_code) {
  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_initialized_ = false;

  // Must reset these to ensure that mouse move/wheel events work with a new
  // renderer.
  mouse_move_pending_ = false;
  next_mouse_move_.reset();
  mouse_wheel_pending_ = false;
  coalesced_mouse_wheel_events_.clear();

  // Must reset these to ensure that keyboard events work with a new renderer.
  key_queue_.clear();
  suppress_next_char_events_ = false;

  // Reset some fields in preparation for recovering from a crash.
  resize_ack_pending_ = false;
  repaint_ack_pending_ = false;

  in_flight_size_.SetSize(0, 0);
  in_flight_reserved_rect_.SetRect(0, 0, 0, 0);
  current_size_.SetSize(0, 0);
  current_reserved_rect_.SetRect(0, 0, 0, 0);
  is_hidden_ = false;
  is_accelerated_compositing_active_ = false;

  if (view_) {
    view_->RenderViewGone(status, exit_code);
    view_ = NULL;  // The View should be deleted by RenderViewGone.
  }

  BackingStoreManager::RemoveBackingStore(this);
}

void RenderWidgetHost::UpdateTextDirection(WebTextDirection direction) {
  text_direction_updated_ = true;
  text_direction_ = direction;
}

void RenderWidgetHost::CancelUpdateTextDirection() {
  if (text_direction_updated_)
    text_direction_canceled_ = true;
}

void RenderWidgetHost::NotifyTextDirection() {
  if (text_direction_updated_) {
    if (!text_direction_canceled_)
      Send(new ViewMsg_SetTextDirection(routing_id(), text_direction_));
    text_direction_updated_ = false;
    text_direction_canceled_ = false;
  }
}

void RenderWidgetHost::SetInputMethodActive(bool activate) {
  Send(new ViewMsg_SetInputMethodActive(routing_id(), activate));
}

void RenderWidgetHost::ImeSetComposition(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  Send(new ViewMsg_ImeSetComposition(
            routing_id(), text, underlines, selection_start, selection_end));
}

void RenderWidgetHost::ImeConfirmComposition(const string16& text) {
  Send(new ViewMsg_ImeConfirmComposition(routing_id(), text));
}

void RenderWidgetHost::ImeConfirmComposition() {
  Send(new ViewMsg_ImeConfirmComposition(routing_id(), string16()));
}

void RenderWidgetHost::ImeCancelComposition() {
  Send(new ViewMsg_ImeSetComposition(routing_id(), string16(),
            std::vector<WebKit::WebCompositionUnderline>(), 0, 0));
}

void RenderWidgetHost::SetActive(bool active) {
  Send(new ViewMsg_SetActive(routing_id(), active));
}

void RenderWidgetHost::Destroy() {
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_DESTROYED,
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

void RenderWidgetHost::CheckRendererIsUnresponsive() {
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
      NotificationType::RENDERER_PROCESS_HANG,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());
  is_unresponsive_ = true;
  NotifyRendererUnresponsive();
}

void RenderWidgetHost::RendererIsResponsive() {
  if (is_unresponsive_) {
    is_unresponsive_ = false;
    NotifyRendererResponsive();
  }
}

void RenderWidgetHost::OnMsgRenderViewReady() {
  WasResized();
}

void RenderWidgetHost::OnMsgRenderViewGone(int status, int exit_code) {
  // TODO(evanm): This synchronously ends up calling "delete this".
  // Is that really what we want in response to this message?  I'm matching
  // previous behavior of the code here.
  Destroy();
}

void RenderWidgetHost::OnMsgClose() {
  Shutdown();
}

void RenderWidgetHost::OnMsgRequestMove(const gfx::Rect& pos) {
  // Note that we ignore the position.
  if (view_) {
    view_->SetSize(pos.size());
    Send(new ViewMsg_Move_ACK(routing_id_));
  }
}

void RenderWidgetHost::OnMsgPaintAtSizeAck(int tag, const gfx::Size& size) {
  PaintAtSizeAckDetails details = {tag, size};
  gfx::Size size_details = size;
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
      Source<RenderWidgetHost>(this),
      Details<PaintAtSizeAckDetails>(&details));
}

void RenderWidgetHost::OnMsgUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  TimeTicks paint_start = TimeTicks::Now();

  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_WILL_PAINT,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // Update our knowledge of the RenderWidget's size.
  current_size_ = params.view_size;
  // Update our knowledge of the RenderWidget's scroll offset.
  last_scroll_offset_ = params.scroll_offset;

  bool is_resize_ack =
      ViewHostMsg_UpdateRect_Flags::is_resize_ack(params.flags);

  // resize_ack_pending_ needs to be cleared before we call DidPaintRect, since
  // that will end up reaching GetBackingStore.
  if (is_resize_ack) {
    DCHECK(resize_ack_pending_);
    resize_ack_pending_ = false;
    in_flight_size_.SetSize(0, 0);
    in_flight_reserved_rect_.SetRect(0, 0, 0, 0);
    // Update our knowledge of the RenderWidget's resizer rect.
    // ViewMsg_Resize is acknowledged only when view size is actually changed,
    // otherwise current_reserved_rect_ is updated immediately after sending
    // ViewMsg_Resize to the RenderWidget and can be clobbered by
    // OnMsgUpdateRect called for a paint that was initiated before the resize
    // message was sent.
    current_reserved_rect_ = params.resizer_rect;
  }

  bool is_repaint_ack =
      ViewHostMsg_UpdateRect_Flags::is_repaint_ack(params.flags);
  if (is_repaint_ack) {
    repaint_ack_pending_ = false;
    TimeDelta delta = TimeTicks::Now() - repaint_start_time_;
    UMA_HISTOGRAM_TIMES("MPArch.RWH_RepaintDelta", delta);
  }

  DCHECK(!params.bitmap_rect.IsEmpty());
  DCHECK(!params.view_size.IsEmpty());

  if (!is_accelerated_compositing_active_) {
    const size_t size = params.bitmap_rect.height() *
        params.bitmap_rect.width() * 4;
    TransportDIB* dib = process_->GetTransportDIB(params.bitmap);

    // If gpu process does painting, scroll_rect and copy_rects are always empty
    // and backing store is never used.
    if (dib) {
      if (dib->size() < size) {
        DLOG(WARNING) << "Transport DIB too small for given rectangle";
        UserMetrics::RecordAction(UserMetricsAction(
            "BadMessageTerminate_RWH1"));
        process()->ReceivedBadMessage();
      } else {
        // Scroll the backing store.
        if (!params.scroll_rect.IsEmpty()) {
          ScrollBackingStoreRect(params.dx, params.dy,
                                 params.scroll_rect,
                                 params.view_size);
        }

        // Paint the backing store. This will update it with the
        // renderer-supplied bits. The view will read out of the backing store
        // later to actually draw to the screen.
        PaintBackingStoreRect(params.bitmap, params.bitmap_rect,
                              params.copy_rects, params.view_size);
      }
    }
  }

  // ACK early so we can prefetch the next PaintRect if there is a next one.
  // This must be done AFTER we're done painting with the bitmap supplied by the
  // renderer. This ACK is a signal to the renderer that the backing store can
  // be re-used, so the bitmap may be invalid after this call.
  Send(new ViewMsg_UpdateRect_ACK(routing_id_));

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // us more data.
  if (is_hidden_)
    return;

  // Now paint the view. Watch out: it might be destroyed already.
  if (view_) {
    view_->MovePluginWindows(params.plugin_window_moves);
    // The view_ pointer could be destroyed in the context of MovePluginWindows
    // which attempts to move the plugin windows and in the process could
    // dispatch other window messages which could cause the view to be
    // destroyed.
    if (view_ && !is_accelerated_compositing_active_) {
      view_being_painted_ = true;
      view_->DidUpdateBackingStore(params.scroll_rect, params.dx, params.dy,
                                   params.copy_rects);
      view_being_painted_ = false;
    }
  }

  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_DID_PAINT,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // If we got a resize ack, then perhaps we have another resize to send?
  if (is_resize_ack && view_) {
    // WasResized checks the current size and sends the resize update only
    // when something was actually changed.
    WasResized();
  }

  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // Log the time delta for processing a paint message.
  TimeDelta delta = TimeTicks::Now() - paint_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgUpdateRect", delta);
}

void RenderWidgetHost::OnMsgInputEventAck(const IPC::Message& message) {
  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_InputEventDelta", delta);

  // Cancel pending hung renderer checks since the renderer is responsive.
  StopHangMonitorTimeout();

  void* iter = NULL;
  int type = 0;
  if (!message.ReadInt(&iter, &type) || (type < WebInputEvent::Undefined)) {
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RWH2"));
    process()->ReceivedBadMessage();
  } else if (type == WebInputEvent::MouseMove) {
    mouse_move_pending_ = false;

    // now, we can send the next mouse move event
    if (next_mouse_move_.get()) {
      DCHECK(next_mouse_move_->type == WebInputEvent::MouseMove);
      ForwardMouseEvent(*next_mouse_move_);
    }
  } else if (type == WebInputEvent::MouseWheel) {
    ProcessWheelAck();
  } else if (WebInputEvent::isKeyboardEventType(type)) {
    bool processed = false;
    if (!message.ReadBool(&iter, &processed)) {
      UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RWH3"));
      process()->ReceivedBadMessage();
    }

    ProcessKeyboardEventAck(type, processed);
  }
  // This is used only for testing.
  NotificationService::current()->Notify(
      NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,
      Source<RenderWidgetHost>(this),
      Details<int>(&type));
}

void RenderWidgetHost::ProcessWheelAck() {
  mouse_wheel_pending_ = false;

  // Now send the next (coalesced) mouse wheel event.
  if (!coalesced_mouse_wheel_events_.empty()) {
    WebMouseWheelEvent next_wheel_event =
        coalesced_mouse_wheel_events_.front();
    coalesced_mouse_wheel_events_.pop_front();
    ForwardWheelEvent(next_wheel_event);
  }
}

void RenderWidgetHost::OnMsgFocus() {
  // Only RenderViewHost can deal with that message.
  UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RWH4"));
  process()->ReceivedBadMessage();
}

void RenderWidgetHost::OnMsgBlur() {
  // Only RenderViewHost can deal with that message.
  UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_RWH5"));
  process()->ReceivedBadMessage();
}

void RenderWidgetHost::OnMsgSetCursor(const WebCursor& cursor) {
  if (!view_) {
    return;
  }
  view_->UpdateCursor(cursor);
}

void RenderWidgetHost::OnMsgImeUpdateTextInputState(
    WebKit::WebTextInputType type,
    const gfx::Rect& caret_rect) {
  if (view_)
    view_->ImeUpdateTextInputState(type, caret_rect);
}

void RenderWidgetHost::OnMsgImeCancelComposition() {
  if (view_)
    view_->ImeCancelComposition();
}

void RenderWidgetHost::OnMsgDidActivateAcceleratedCompositing(bool activated) {
#if defined(OS_MACOSX)
  bool old_state = is_accelerated_compositing_active_;
#endif
  is_accelerated_compositing_active_ = activated;
#if defined(OS_MACOSX)
  if (old_state != is_accelerated_compositing_active_ && view_)
    view_->GpuRenderingStateDidChange();
#elif defined(OS_WIN)
  if (view_)
    view_->ShowCompositorHostWindow(is_accelerated_compositing_active_);
#elif defined(TOOLKIT_USES_GTK)
  if (view_)
    view_->AcceleratedCompositingActivated(activated);
#endif
}

#if defined(OS_MACOSX)

void RenderWidgetHost::OnMsgGetScreenInfo(gfx::NativeViewId view,
                                          WebScreenInfo* results) {
  gfx::NativeView native_view = view_ ? view_->GetNativeView() : NULL;
  *results = WebScreenInfoFactory::screenInfo(native_view);
}

void RenderWidgetHost::OnMsgGetWindowRect(gfx::NativeViewId window_id,
                                          gfx::Rect* results) {
  if (view_) {
    *results = view_->GetViewBounds();
  }
}

void RenderWidgetHost::OnMsgGetRootWindowRect(gfx::NativeViewId window_id,
                                              gfx::Rect* results) {
  if (view_) {
    *results = view_->GetRootWindowRect();
  }
}

void RenderWidgetHost::OnMsgPluginFocusChanged(bool focused, int plugin_id) {
  if (view_)
    view_->PluginFocusChanged(focused, plugin_id);
}

void RenderWidgetHost::OnMsgStartPluginIme() {
  if (view_)
    view_->StartPluginIme();
}

void RenderWidgetHost::OnAllocateFakePluginWindowHandle(
    bool opaque,
    bool root,
    gfx::PluginWindowHandle* id) {
  // TODO(kbr): similar potential issue here as in OnMsgCreatePluginContainer.
  // Possibly less of an issue because this is only used for the GPU plugin.
  if (view_) {
    *id = view_->AllocateFakePluginWindowHandle(opaque, root);
  } else {
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHost::OnDestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  if (view_) {
    view_->DestroyFakePluginWindowHandle(id);
  } else {
    NOTIMPLEMENTED();
  }
}

void RenderWidgetHost::OnAcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 mach_port) {
  if (view_) {
    view_->AcceleratedSurfaceSetIOSurface(window, width, height, mach_port);
  }
}

void RenderWidgetHost::OnAcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  if (view_) {
    view_->AcceleratedSurfaceSetTransportDIB(window, width, height,
                                             transport_dib);
  }
}

void RenderWidgetHost::OnAcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window, uint64 surface_id) {
  if (view_) {
    // This code path could be updated to implement flow control for
    // updating of accelerated plugins as well. However, if we add support
    // for composited plugins then this is not necessary.
    view_->AcceleratedSurfaceBuffersSwapped(window, surface_id,
                                            0, 0, 0, 0);
  }
}
#elif defined(OS_POSIX)

void RenderWidgetHost::OnMsgCreatePluginContainer(gfx::PluginWindowHandle id) {
  // TODO(piman): view_ can only be NULL with delayed view creation in
  // extensions (see ExtensionHost::CreateRenderViewSoon). Figure out how to
  // support plugins in that case.
  if (view_) {
    view_->CreatePluginContainer(id);
  } else {
    deferred_plugin_handles_.push_back(id);
  }
}

void RenderWidgetHost::OnMsgDestroyPluginContainer(gfx::PluginWindowHandle id) {
  if (view_) {
    view_->DestroyPluginContainer(id);
  } else {
    for (int i = 0;
         i < static_cast<int>(deferred_plugin_handles_.size());
         i++) {
      if (deferred_plugin_handles_[i] == id) {
        deferred_plugin_handles_.erase(deferred_plugin_handles_.begin() + i);
        i--;
      }
    }
  }
}
#endif

void RenderWidgetHost::PaintBackingStoreRect(
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    const gfx::Size& view_size) {
  // The view may be destroyed already.
  if (!view_)
    return;

  if (is_hidden_) {
    // Don't bother updating the backing store when we're hidden. Just mark it
    // as being totally invalid. This will cause a complete repaint when the
    // view is restored.
    needs_repainting_on_restore_ = true;
    return;
  }

  bool needs_full_paint = false;
  BackingStoreManager::PrepareBackingStore(this, view_size, bitmap, bitmap_rect,
                                           copy_rects, &needs_full_paint);
  if (needs_full_paint) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }
}

void RenderWidgetHost::ScrollBackingStoreRect(int dx, int dy,
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
  backing_store->ScrollBackingStore(dx, dy, clip_rect, view_size);
}

void RenderWidgetHost::ToggleSpellPanel(bool is_currently_visible) {
  Send(new ViewMsg_ToggleSpellPanel(routing_id(), is_currently_visible));
}

void RenderWidgetHost::Replace(const string16& word) {
  Send(new ViewMsg_Replace(routing_id_, word));
}

void RenderWidgetHost::AdvanceToNextMisspelling() {
  Send(new ViewMsg_AdvanceToNextMisspelling(routing_id_));
}

void RenderWidgetHost::EnableRendererAccessibility() {
  if (renderer_accessible_)
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }

  renderer_accessible_ = true;

  if (process_->HasConnection()) {
    // Renderer accessibility wasn't enabled on process launch. Enable it now.
    Send(new ViewMsg_EnableAccessibility(routing_id()));
  }
}

void RenderWidgetHost::SetAccessibilityFocus(int acc_obj_id) {
  Send(new ViewMsg_SetAccessibilityFocus(routing_id(), acc_obj_id));
}

void RenderWidgetHost::AccessibilityDoDefaultAction(int acc_obj_id) {
  Send(new ViewMsg_AccessibilityDoDefaultAction(routing_id(), acc_obj_id));
}

void RenderWidgetHost::AccessibilityNotificationsAck() {
  Send(new ViewMsg_AccessibilityNotifications_ACK(routing_id()));
}

void RenderWidgetHost::ProcessKeyboardEventAck(int type, bool processed) {
  if (key_queue_.size() == 0) {
    LOG(ERROR) << "Got a KeyEvent back from the renderer but we "
               << "don't seem to have sent it to the renderer!";
  } else if (key_queue_.front().type != type) {
    LOG(ERROR) << "We seem to have a different key type sent from "
               << "the renderer. (" << key_queue_.front().type << " vs. "
               << type << "). Ignoring event.";

    // Something must be wrong. Clear the |key_queue_| and
    // |suppress_next_char_events_| so that we can resume from the error.
    key_queue_.clear();
    suppress_next_char_events_ = false;
  } else {
    NativeWebKeyboardEvent front_item = key_queue_.front();
    key_queue_.pop_front();

#if defined(OS_MACOSX)
    if (!is_hidden_ && view_->PostProcessEventForPluginIme(front_item))
      return;
#endif

    // We only send unprocessed key event upwards if we are not hidden,
    // because the user has moved away from us and no longer expect any effect
    // of this key event.
    if (!processed && !is_hidden_ && !front_item.skip_in_browser) {
      UnhandledKeyboardEvent(front_item);

      // WARNING: This RenderWidgetHost can be deallocated at this point
      // (i.e.  in the case of Ctrl+W, where the call to
      // UnhandledKeyboardEvent destroys this RenderWidgetHost).
    }
  }
}

void RenderWidgetHost::ActivateDeferredPluginHandles() {
  if (view_ == NULL)
    return;

  for (int i = 0; i < static_cast<int>(deferred_plugin_handles_.size()); i++) {
#if defined(TOOLKIT_USES_GTK)
    view_->CreatePluginContainer(deferred_plugin_handles_[i]);
#endif
  }

  deferred_plugin_handles_.clear();
}
