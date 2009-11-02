// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host.h"

#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/render_widget_host_painting_observer.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "webkit/glue/webcursor.h"

#if defined(TOOLKIT_VIEWS)
#include "views/view.h"
#endif

#if defined (OS_MACOSX)
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/mac/WebScreenInfoFactory.h"
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

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHost

RenderWidgetHost::RenderWidgetHost(RenderProcessHost* process,
                                   int routing_id)
    : renderer_initialized_(false),
      view_(NULL),
      process_(process),
      painting_observer_(NULL),
      routing_id_(routing_id),
      is_loading_(false),
      is_hidden_(false),
      repaint_ack_pending_(false),
      resize_ack_pending_(false),
      mouse_move_pending_(false),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      in_get_backing_store_(false),
      view_being_painted_(false),
      text_direction_updated_(false),
      text_direction_(WebKit::WebTextDirectionLeftToRight),
      text_direction_canceled_(false),
      pending_key_events_(false),
      suppress_next_char_events_(false),
      death_flag_(NULL) {
  if (routing_id_ == MSG_ROUTING_NONE)
    routing_id_ = process_->GetNextRoutingID();

  process_->Attach(this, routing_id_);
  // Because the widget initializes as is_hidden_ == false,
  // tell the process host that we're alive.
  process_->WidgetRestored();
}

RenderWidgetHost::~RenderWidgetHost() {
  // Force the method that destroys this object to exit immediately.
  if (death_flag_)
    *death_flag_ = true;

  // Clear our current or cached backing store if either remains.
  BackingStoreManager::RemoveBackingStore(this);

  process_->Release(routing_id_);
}

gfx::NativeViewId RenderWidgetHost::GetNativeViewId() {
  if (view_)
    return gfx::IdFromNativeView(view_->GetNativeView());
  return NULL;
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

void RenderWidgetHost::OnMessageReceived(const IPC::Message &msg) {
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderWidgetHost, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnMsgRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnMsgRenderViewGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnMsgClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnMsgRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PaintRect, OnMsgPaintRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ScrollRect, OnMsgScrollRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnMsgInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnMsgFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Blur, OnMsgBlur)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnMsgFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnMsgSetCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeUpdateStatus, OnMsgImeUpdateStatus)
#if defined(OS_LINUX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreatePluginContainer,
                        OnMsgCreatePluginContainer)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DestroyPluginContainer,
                        OnMsgDestroyPluginContainer)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnMsgShowPopup)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetScreenInfo, OnMsgGetScreenInfo)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetWindowRect, OnMsgGetWindowRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetRootWindowRect, OnMsgGetRootWindowRect)
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message de-serialization failed. Kill the renderer process.
    process()->ReceivedBadMessage(msg.type());
  }
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
  bool needs_repainting;
  if (needs_repainting_on_restore_ || !backing_store) {
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
}

void RenderWidgetHost::WasResized() {
  if (resize_ack_pending_ || !process_->HasConnection() || !view_ ||
      !renderer_initialized_) {
    return;
  }

  gfx::Rect view_bounds = view_->GetViewBounds();
  gfx::Size new_size(view_bounds.width(), view_bounds.height());

  // Avoid asking the RenderWidget to resize to its current size, since it
  // won't send us a PaintRect message in that case.
  if (new_size == current_size_)
    return;

  if (in_flight_size_ != gfx::Size() && new_size == in_flight_size_)
    return;

  // We don't expect to receive an ACK when the requested size is empty.
  if (!new_size.IsEmpty())
    resize_ack_pending_ = true;

  if (!Send(new ViewMsg_Resize(routing_id_, new_size,
                               GetRootWindowResizerRect())))
    resize_ack_pending_ = false;
  else
    in_flight_size_ = new_size;
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
  in_get_backing_store_ = true;

  // We might have a cached backing store that we can reuse!
  BackingStore* backing_store =
      BackingStoreManager::GetBackingStore(this, current_size_);
  if (!force_create) {
    in_get_backing_store_ = false;
    return backing_store;
  }

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
    if (process_->WaitForPaintMsg(routing_id_, max_delay, &msg)) {
      ViewHostMsg_PaintRect::Dispatch(
          &msg, this, &RenderWidgetHost::OnMsgPaintRect);
      backing_store = BackingStoreManager::GetBackingStore(this, current_size_);
    }
  }

  in_get_backing_store_ = false;
  return backing_store;
}

BackingStore* RenderWidgetHost::AllocBackingStore(const gfx::Size& size) {
  if (!view_)
    return NULL;
  return view_->AllocBackingStore(size);
}

void RenderWidgetHost::StartHangMonitorTimeout(TimeDelta delay) {
  time_when_considered_hung_ = Time::Now() + delay;

  // If we already have a timer that will expire at or before the given delay,
  // then we have nothing more to do now.
  if (hung_renderer_timer_.IsRunning() &&
      hung_renderer_timer_.GetCurrentDelay() <= delay)
    return;

  // Either the timer is not yet running, or we need to adjust the timer to
  // fire sooner.
  hung_renderer_timer_.Stop();
  hung_renderer_timer_.Start(delay, this,
      &RenderWidgetHost::CheckRendererIsUnresponsive);
}

void RenderWidgetHost::RestartHangMonitorTimeout() {
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
  if (process_->ignore_input_events())
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

  ForwardInputEvent(mouse_event, sizeof(WebMouseEvent));
}

void RenderWidgetHost::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  if (process_->ignore_input_events())
    return;

  ForwardInputEvent(wheel_event, sizeof(WebMouseWheelEvent));
}

void RenderWidgetHost::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (process_->ignore_input_events())
    return;

  if (key_event.type == WebKeyboardEvent::Char &&
      (key_event.windowsKeyCode == base::VKEY_RETURN ||
       key_event.windowsKeyCode == base::VKEY_SPACE)) {
    OnUserGesture();
  }

  // Double check the type to make sure caller hasn't sent us nonsense that
  // will mess up our key queue.
  if (WebInputEvent::isKeyboardEventType(key_event.type)) {
    // Don't add this key to the queue if we have no way to send the message...
    if (!process_->HasConnection())
      return;

    // To help understand following logic, please refer to the comments
    // explaining |pending_key_events_| and |suppress_next_char_events_|
    // members. And the comments in OnMsgInputEventAck() method, which contains
    // the bottom half of the logic.
    //
    // There are to different situations for us to handle key events:
    // 1. After sending a key event to the renderer, we receive its ACK message
    // from the renderer before sending the next key event.
    // In this case, there is always no more than 1 key event in |key_queue_|,
    // and we send the key event one by one in this method, except that a
    // sequence of Char events may be suppressed directly if the preceding
    // RawKeyDown event was handled as an accelerator key in the browser.
    //
    // 2. We get the next key event before receving the previous one's ACK
    // message from the renderer.
    // In this case, when we get a key event, the previous key event is still in
    // |keq_queue_| waiting for being handled in OnMsgInputEventAck() method.
    // Then we need handle following cases differently:
    //   1) If we get a Char event, then the previous key event in |key_queue_|
    //   waiting for ACK must be a RawKeyDown event. Then we need keep this Char
    //   event in |key_queue_| rather than sending it immediately, because we
    //   can't determine if we should send it to the renderer or just suppress
    //   it, until we receive the preceding RawKeyDown event's ACK message.
    //   We increase the count of |pending_key_events_| to help remember this
    //   Char event is not sent to the renderer yet.
    //   2) If there is already one or more key event pending in |key_queue_|
    //   (|pending_key_events_| > 0), we can not send a new key event
    //   immediately no matter what type it is. Otherwise the key events will be
    //   in wrong order. In this case we just keep them in |key_queue_| and
    //   increase |pending_key_events_| to remember them.
    //
    // Then all pending key events in |key_queue_| will be handled properly in
    // OnMsgInputEventAck() method. Please refer to that method for details.

    // Tab switching/closing accelerators aren't sent to the renderer to avoid a
    // hung/malicious renderer from interfering.
    if (!ShouldSendToRenderer(key_event)) {
      UnhandledKeyboardEvent(key_event);
      if (key_event.type == WebKeyboardEvent::RawKeyDown)
        suppress_next_char_events_ = true;
      return;
    }

    bool is_char = (key_event.type == WebKeyboardEvent::Char);

    // Handle the first situation mentioned above.
    if (suppress_next_char_events_) {
      // If preceding RawKeyDown event was handled by the browser, then we need
      // suppress all Char events generated by it. Please note that, one
      // RawKeyDown event may generate multiple Char events, so we can't reset
      // |suppress_next_char_events_| until we get a KeyUp or a RawKeyDown.
      if (is_char)
        return;
      // We get a KeyUp or a RawKeyDown event.
      suppress_next_char_events_ = false;
    }

    // Put all WebKeyboardEvent objects in a queue since we can't trust the
    // renderer and we need to give something to the UnhandledInputEvent
    // handler.
    key_queue_.push_back(key_event);
    HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

    // Handle the second situation mentioned above.
    size_t key_queue_size = key_queue_.size();
    if ((is_char && key_queue_size > 1) || pending_key_events_) {
      // The event is not send to the renderer, increase |pending_key_events_|
      // to remember it.
      ++pending_key_events_;

      // Some sanity checks.
      // At least one key event in the front of |key_queue_| has been sent to
      // the renderer, otherwise we won't be able to get any ACK back from the
      // renderer.
      DCHECK(key_queue_size > pending_key_events_);
      // Char events must be preceded by RawKeyDown events.
      // TODO(suzhe): verify it on all platforms.
      DCHECK(key_queue_[key_queue_size - pending_key_events_ - 1].type ==
             WebKeyboardEvent::RawKeyDown);
      // The first pending key event must be a Char event. Other key events must
      // already be sent to the renderer at this point.
      DCHECK(key_queue_[key_queue_size - pending_key_events_].type ==
             WebKeyboardEvent::Char);
    } else {
      // Fall into the first situation, so we can send the event immediately.
      // Only forward the non-native portions of our event.
      ForwardInputEvent(key_event, sizeof(WebKeyboardEvent));
    }
  }
}

void RenderWidgetHost::ForwardInputEvent(const WebInputEvent& input_event,
                                         int event_size) {
  if (!process_->HasConnection())
    return;

  DCHECK(!process_->ignore_input_events());

  IPC::Message* message = new ViewMsg_HandleInputEvent(routing_id_);
  message->WriteData(
      reinterpret_cast<const char*>(&input_event), event_size);
  input_event_start_time_ = TimeTicks::Now();
  Send(message);

  // Any input event cancels a pending mouse move event.
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

void RenderWidgetHost::RendererExited() {
  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_initialized_ = false;

  // Must reset these to ensure that mouse move events work with a new renderer.
  mouse_move_pending_ = false;
  next_mouse_move_.reset();

  // Reset some fields in preparation for recovering from a crash.
  resize_ack_pending_ = false;
  in_flight_size_.SetSize(0, 0);
  current_size_.SetSize(0, 0);
  is_hidden_ = false;

  if (view_) {
    view_->RenderViewGone();
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

void RenderWidgetHost::ImeSetInputMode(bool activate) {
  Send(new ViewMsg_ImeSetInputMode(routing_id(), activate));
}

void RenderWidgetHost::ImeSetComposition(const string16& ime_string,
                                         int cursor_position,
                                         int target_start,
                                         int target_end) {
  Send(new ViewMsg_ImeSetComposition(routing_id(),
                                     WebKit::WebCompositionCommandSet,
                                     cursor_position, target_start, target_end,
                                     ime_string));
}

void RenderWidgetHost::ImeConfirmComposition(const string16& ime_string) {
  Send(new ViewMsg_ImeSetComposition(routing_id(),
                                     WebKit::WebCompositionCommandConfirm,
                                     -1, -1, -1, ime_string));
}

void RenderWidgetHost::ImeCancelComposition() {
  Send(new ViewMsg_ImeSetComposition(routing_id(),
                                     WebKit::WebCompositionCommandDiscard,
                                     -1, -1, -1, string16()));
}

gfx::Rect RenderWidgetHost::GetRootWindowResizerRect() const {
  return gfx::Rect();
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

void RenderWidgetHost::OnMsgRenderViewGone() {
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

void RenderWidgetHost::OnMsgPaintRect(
    const ViewHostMsg_PaintRect_Params& params) {
  TimeTicks paint_start = TimeTicks::Now();

  // Update our knowledge of the RenderWidget's size.
  current_size_ = params.view_size;

  bool is_resize_ack =
      ViewHostMsg_PaintRect_Flags::is_resize_ack(params.flags);

  // resize_ack_pending_ needs to be cleared before we call DidPaintRect, since
  // that will end up reaching GetBackingStore.
  if (is_resize_ack) {
    DCHECK(resize_ack_pending_);
    resize_ack_pending_ = false;
    in_flight_size_.SetSize(0, 0);
  }

  bool is_repaint_ack =
      ViewHostMsg_PaintRect_Flags::is_repaint_ack(params.flags);
  if (is_repaint_ack) {
    repaint_ack_pending_ = false;
    TimeDelta delta = TimeTicks::Now() - repaint_start_time_;
    UMA_HISTOGRAM_TIMES("MPArch.RWH_RepaintDelta", delta);
  }

  DCHECK(!params.bitmap_rect.IsEmpty());
  DCHECK(!params.view_size.IsEmpty());

  const size_t size = params.bitmap_rect.height() *
                      params.bitmap_rect.width() * 4;
  TransportDIB* dib = process_->GetTransportDIB(params.bitmap);
  if (dib) {
    if (dib->size() < size) {
      DLOG(WARNING) << "Transport DIB too small for given rectangle";
      process()->ReceivedBadMessage(ViewHostMsg_PaintRect__ID);
    } else {
      // Paint the backing store. This will update it with the renderer-supplied
      // bits. The view will read out of the backing store later to actually
      // draw to the screen.
      PaintBackingStoreRect(dib, params.bitmap_rect, params.view_size);
    }
  }

  // ACK early so we can prefetch the next PaintRect if there is a next one.
  // This must be done AFTER we're done painting with the bitmap supplied by the
  // renderer. This ACK is a signal to the renderer that the backing store can
  // be re-used, so the bitmap may be invalid after this call.
  Send(new ViewMsg_PaintRect_ACK(routing_id_));

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // us more data.
  if (is_hidden_)
    return;

  // Now paint the view. Watch out: it might be destroyed already.
  if (view_) {
    view_->MovePluginWindows(params.plugin_window_moves);
    view_being_painted_ = true;
    view_->DidPaintRect(params.bitmap_rect);
    view_being_painted_ = false;
  }

  if (paint_observer_.get())
    paint_observer_->RenderWidgetHostDidPaint(this);

  // If we got a resize ack, then perhaps we have another resize to send?
  if (is_resize_ack && view_) {
    gfx::Rect view_bounds = view_->GetViewBounds();
    if (current_size_.width() != view_bounds.width() ||
        current_size_.height() != view_bounds.height()) {
      WasResized();
    }
  }

  if (painting_observer_)
    painting_observer_->WidgetDidUpdateBackingStore(this);

  // Log the time delta for processing a paint message.
  TimeDelta delta = TimeTicks::Now() - paint_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgPaintRect", delta);
}

void RenderWidgetHost::OnMsgScrollRect(
    const ViewHostMsg_ScrollRect_Params& params) {
  TimeTicks scroll_start = TimeTicks::Now();

  DCHECK(!params.view_size.IsEmpty());

  const size_t size = params.bitmap_rect.height() *
                      params.bitmap_rect.width() * 4;
  TransportDIB* dib = process_->GetTransportDIB(params.bitmap);
  if (dib) {
    if (dib->size() < size) {
      LOG(WARNING) << "Transport DIB too small for given rectangle";
      process()->ReceivedBadMessage(ViewHostMsg_PaintRect__ID);
    } else {
      // Scroll the backing store.
      ScrollBackingStoreRect(dib, params.bitmap_rect,
                             params.dx, params.dy,
                             params.clip_rect, params.view_size);
    }
  }

  // ACK early so we can prefetch the next ScrollRect if there is a next one.
  // This must be done AFTER we're done painting with the bitmap supplied by the
  // renderer. This ACK is a signal to the renderer that the backing store can
  // be re-used, so the bitmap may be invalid after this call.
  Send(new ViewMsg_ScrollRect_ACK(routing_id_));

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // is more data.
  if (is_hidden_)
    return;

  // Paint the view. Watch out: it might be destroyed already.
  if (view_) {
    view_being_painted_ = true;
    view_->MovePluginWindows(params.plugin_window_moves);
    view_->DidScrollRect(params.clip_rect, params.dx, params.dy);
    view_being_painted_ = false;
  }

  if (painting_observer_)
    painting_observer_->WidgetDidUpdateBackingStore(this);

  // Log the time delta for processing a scroll message.
  TimeDelta delta = TimeTicks::Now() - scroll_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgScrollRect", delta);
}

void RenderWidgetHost::OnMsgInputEventAck(const IPC::Message& message) {
  // Track if |this| is destroyed or not.
  bool is_dead = false;
  death_flag_ = &is_dead;

  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_InputEventDelta", delta);

  // Cancel pending hung renderer checks since the renderer is responsive.
  StopHangMonitorTimeout();

  void* iter = NULL;
  int type = 0;
  if (!message.ReadInt(&iter, &type) || (type < WebInputEvent::Undefined))
    process()->ReceivedBadMessage(message.type());

  if (type == WebInputEvent::MouseMove) {
    mouse_move_pending_ = false;

    // now, we can send the next mouse move event
    if (next_mouse_move_.get()) {
      DCHECK(next_mouse_move_->type == WebInputEvent::MouseMove);
      ForwardMouseEvent(*next_mouse_move_);
    }
  }

  if (WebInputEvent::isKeyboardEventType(type)) {
    if (key_queue_.size() == 0) {
      LOG(ERROR) << "Got a KeyEvent back from the renderer but we "
                 << "don't seem to have sent it to the renderer!";
    } else if (key_queue_.front().type != type) {
      LOG(ERROR) << "We seem to have a different key type sent from "
                 << "the renderer. (" << key_queue_.front().type << " vs. "
                 << type << "). Ignoring event.";

      // Something must be wrong. |key_queue_| must be cleared here to make sure
      // the feedback loop for sending upcoming keyboard events can be resumed
      // correctly.
      key_queue_.clear();
      pending_key_events_ = 0;
      suppress_next_char_events_ = false;
    } else {
      bool processed = false;
      if (!message.ReadBool(&iter, &processed))
        process()->ReceivedBadMessage(message.type());

      NativeWebKeyboardEvent front_item = key_queue_.front();
      key_queue_.pop_front();

      bool processed_by_browser = false;
      if (!processed) {
        processed_by_browser = UnhandledKeyboardEvent(front_item);

        // WARNING: This RenderWidgetHost can be deallocated at this point
        // (i.e.  in the case of Ctrl+W, where the call to
        // UnhandledKeyboardEvent destroys this RenderWidgetHost).
      }

      // This RenderWidgetHost was already deallocated, we can't do anything
      // from now on, including resetting |death_flag_|. So just return.
      if (is_dead)
        return;

      // Suppress the following Char events if the RawKeyDown event was handled
      // by the browser rather than the renderer.
      if (front_item.type == WebKeyboardEvent::RawKeyDown)
        suppress_next_char_events_ = processed_by_browser;

      // If more than one key events in |key_queue_| were already sent to the
      // renderer but haven't got ACK messages yet, we must wait for ACK
      // messages of these key events before sending more key events to the
      // renderer.
      if (pending_key_events_ && key_queue_.size() == pending_key_events_) {
        size_t i = 0;
        if (suppress_next_char_events_) {
          // Suppress the sequence of pending Char events if preceding
          // RawKeyDown event was handled by the browser.
          while (pending_key_events_ &&
                 key_queue_[0].type == WebKeyboardEvent::Char) {
            --pending_key_events_;
            key_queue_.pop_front();
          }
        } else {
          // Otherwise, send these pending Char events to the renderer.
          // Note: we can't remove them from |key_queue_|, as we still need to
          // wait for their ACK messages from the renderer.
          while (pending_key_events_ &&
                 key_queue_[i].type == WebKeyboardEvent::Char) {
            --pending_key_events_;
            ForwardInputEvent(key_queue_[i++], sizeof(WebKeyboardEvent));
          }
        }

        // Reset |suppress_next_char_events_| if there is still one or more
        // pending KeyUp or RawKeyDown events in the queue.
        // We can't reset it if there is no pending event anymore, because we
        // don't know if the following event is a Char event or not.
        if (pending_key_events_)
          suppress_next_char_events_ = false;

        // We can safely send following pending KeyUp and RawKeyDown events to
        // the renderer, until we meet another Char event.
        while (pending_key_events_ &&
               key_queue_[i].type != WebKeyboardEvent::Char) {
          --pending_key_events_;
          ForwardInputEvent(key_queue_[i++], sizeof(WebKeyboardEvent));
        }
      }
    }
  }

  // Reset |death_flag_| to NULL, otherwise it'll point to an invalid memory
  // address after returning from this method.
  death_flag_ = NULL;
}

void RenderWidgetHost::OnMsgFocus() {
  // Only the user can focus a RenderWidgetHost.
  process()->ReceivedBadMessage(ViewHostMsg_Focus__ID);
}

void RenderWidgetHost::OnMsgBlur() {
  if (view_) {
    view_->Blur();
  }
}

void RenderWidgetHost::OnMsgFocusedNodeChanged() {
}

void RenderWidgetHost::OnMsgSetCursor(const WebCursor& cursor) {
  if (!view_) {
    return;
  }
  view_->UpdateCursor(cursor);
}

void RenderWidgetHost::OnMsgImeUpdateStatus(int control,
                                            const gfx::Rect& caret_rect) {
  if (view_) {
    view_->IMEUpdateStatus(control, caret_rect);
  }
}

#if defined(OS_LINUX)

void RenderWidgetHost::OnMsgCreatePluginContainer(gfx::PluginWindowHandle id) {
  view_->CreatePluginContainer(id);
}

void RenderWidgetHost::OnMsgDestroyPluginContainer(gfx::PluginWindowHandle id) {
  view_->DestroyPluginContainer(id);
}

#elif defined(OS_MACOSX)

void RenderWidgetHost::OnMsgShowPopup(
    const ViewHostMsg_ShowPopup_Params& params) {
  view_->ShowPopupWithItems(params.bounds,
                            params.item_height,
                            params.selected_item,
                            params.popup_items);
}

void RenderWidgetHost::OnMsgGetScreenInfo(gfx::NativeViewId view,
                                          WebScreenInfo* results) {
  gfx::NativeView native_view = view_ ? view_->GetNativeView() : NULL;
  *results = WebScreenInfoFactory::screenInfo(native_view);
}

void RenderWidgetHost::OnMsgGetWindowRect(gfx::NativeViewId window_id,
                                          gfx::Rect* results) {
  if (view_) {
    *results = view_->GetWindowRect();
  }
}

void RenderWidgetHost::OnMsgGetRootWindowRect(gfx::NativeViewId window_id,
                                              gfx::Rect* results) {
  if (view_) {
    *results = view_->GetRootWindowRect();
  }
}

#endif

void RenderWidgetHost::PaintBackingStoreRect(TransportDIB* bitmap,
                                             const gfx::Rect& bitmap_rect,
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
  BackingStoreManager::PrepareBackingStore(this, view_size,
                                           process_->process().handle(),
                                           bitmap, bitmap_rect,
                                           &needs_full_paint);
  if (needs_full_paint) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }
}

void RenderWidgetHost::ScrollBackingStoreRect(TransportDIB* bitmap,
                                              const gfx::Rect& bitmap_rect,
                                              int dx, int dy,
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
  backing_store->ScrollRect(process_->process().handle(), bitmap, bitmap_rect,
                            dx, dy, clip_rect, view_size);
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
