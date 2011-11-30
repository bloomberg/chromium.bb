// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/compositor_thread.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenuInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/transport_dib.h"
#include "ui/gfx/gl/gl_switches.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#endif  // defined(OS_POSIX)

#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

using WebKit::WebCompositionUnderline;
using WebKit::WebCursorInfo;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebNavigationPolicy;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebPopupMenuInfo;
using WebKit::WebPopupType;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSize;
using WebKit::WebTextDirection;
using WebKit::WebTouchEvent;
using WebKit::WebVector;
using WebKit::WebWidget;
using content::RenderThread;

RenderWidget::RenderWidget(WebKit::WebPopupType popup_type)
    : routing_id_(MSG_ROUTING_NONE),
      webwidget_(NULL),
      opener_id_(MSG_ROUTING_NONE),
      host_window_(0),
      current_paint_buf_(NULL),
      next_paint_flags_(0),
      filtered_time_per_frame_(0.0f),
      update_reply_pending_(false),
      using_asynchronous_swapbuffers_(false),
      num_swapbuffers_complete_pending_(0),
      did_show_(false),
      is_hidden_(false),
      is_fullscreen_(false),
      needs_repainting_on_restore_(false),
      has_focus_(false),
      handling_input_event_(false),
      closing_(false),
      is_swapped_out_(false),
      input_method_is_active_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      can_compose_inline_(true),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      is_accelerated_compositing_active_(false),
      animation_update_pending_(false),
      animation_task_posted_(false),
      invalidation_task_posted_(false) {
  RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());
  has_disable_gpu_vsync_switch_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuVsync);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }
  // If we are swapped out, we have released already.
  if (!is_swapped_out_)
    RenderProcess::current()->ReleaseProcess();
}

// static
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   WebKit::WebPopupType popup_type) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget(new RenderWidget(popup_type));
  widget->Init(opener_id);  // adds reference
  return widget;
}

// static
WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case WebKit::WebPopupTypeNone:  // Nothing to create.
      break;
    case WebKit::WebPopupTypeSelect:
    case WebKit::WebPopupTypeSuggestion:
      return WebPopupMenu::create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

void RenderWidget::Init(int32 opener_id) {
  DoInit(opener_id,
         RenderWidget::CreateWebWidget(this),
         new ViewHostMsg_CreateWidget(opener_id, popup_type_, &routing_id_));
}

void RenderWidget::DoInit(int32 opener_id,
                          WebWidget* web_widget,
                          IPC::SyncMessage* create_widget_message) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = web_widget;

  bool result = RenderThread::Get()->Send(create_widget_message);
  if (result) {
    RenderThread::Get()->AddRoute(routing_id_, this);
    // Take a reference on behalf of the RenderThread.  This will be balanced
    // when we receive ViewMsg_Close.
    AddRef();
  } else {
    DCHECK(false);
  }
}

// This is used to complete pending inits and non-pending inits. For non-
// pending cases, the parent will be the same as the current parent. This
// indicates we do not need to reparent or anything.
void RenderWidget::CompleteInit(gfx::NativeViewId parent_hwnd) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  host_window_ = parent_hwnd;

  Send(new ViewHostMsg_RenderViewReady(routing_id_));
}

void RenderWidget::SetSwappedOut(bool is_swapped_out) {
  // We should only toggle between states.
  DCHECK(is_swapped_out_ != is_swapped_out);
  is_swapped_out_ = is_swapped_out;

  // If we are swapping out, we will call ReleaseProcess, allowing the process
  // to exit if all of its RenderViews are swapped out.  We wait until the
  // WasSwappedOut call to do this, to avoid showing the sad tab.
  // If we are swapping in, we call AddRefProcess to prevent the process from
  // exiting.
  if (!is_swapped_out)
    RenderProcess::current()->AddRefProcess();
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasRestored, OnWasRestored)
    IPC_MESSAGE_HANDLER(ViewMsg_WasSwappedOut, OnWasSwappedOut)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateRect_ACK, OnUpdateRectAck)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(ViewMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(ViewMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInputMethodActive, OnSetInputMethodActive)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeConfirmComposition, OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_PaintAtSize, OnMsgPaintAtSize)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnMsgRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close, and filter
  // most outgoing messages while swapped out.
  if ((is_swapped_out_ &&
       !content::SwappedOutMessages::CanSendWhileSwappedOut(message)) ||
      closing_) {
    delete message;
    return false;
  }

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return RenderThread::Get()->Send(message);
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderWidget::OnCreatingNewAck(
    gfx::NativeViewId parent) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  CompleteInit(parent);
}

void RenderWidget::OnClose() {
  if (closing_)
    return;
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    RenderThread::Get()->RemoveRoute(routing_id_);
    SetHidden(false);
  }

  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  MessageLoop::current()->PostNonNestableTask(
      FROM_HERE, NewRunnableMethod(this, &RenderWidget::Close));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

void RenderWidget::OnResize(const gfx::Size& new_size,
                            const gfx::Rect& resizer_rect,
                            bool is_fullscreen) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // We shouldn't be asked to resize to our current size.
  DCHECK(size_ != new_size || resizer_rect_ != resizer_rect);

  // Remember the rect where the resize corner will be drawn.
  resizer_rect_ = resizer_rect;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change = is_fullscreen_ != is_fullscreen;
  if (fullscreen_change)
    WillToggleFullscreen();
  is_fullscreen_ = is_fullscreen;

  if (size_ != new_size) {
    // TODO(darin): We should not need to reset this here.
    SetHidden(false);
    needs_repainting_on_restore_ = false;

    size_ = new_size;

    // We should not be sent a Resize message if we have not ACK'd the previous
    DCHECK(!next_paint_is_resize_ack());

    paint_aggregator_.ClearPendingUpdate();

    // When resizing, we want to wait to paint before ACK'ing the resize.  This
    // ensures that we only resize as fast as we can paint.  We only need to
    // send an ACK if we are resized to a non-empty rect.
    webwidget_->resize(new_size);
    if (!new_size.IsEmpty()) {
      if (!is_accelerated_compositing_active_) {
        // Resize should have caused an invalidation of the entire view.
        DCHECK(paint_aggregator_.HasPendingUpdate());
      }

      // We will send the Resize_ACK flag once we paint again.
      set_next_paint_is_resize_ack();
    }
  }

  if (fullscreen_change)
    DidToggleFullscreen();
}

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
}

void RenderWidget::OnWasRestored(bool needs_repainting) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasRestored");
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  SetHidden(false);

  if (!needs_repainting && !needs_repainting_on_restore_)
    return;
  needs_repainting_on_restore_ = false;

  // Tag the next paint as a restore ack, which is picked up by
  // DoDeferredUpdate when it sends out the next PaintRect message.
  set_next_paint_is_restore_ack();

  // Generate a full repaint.
  if (!is_accelerated_compositing_active_) {
    didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
  } else {
    scheduleComposite();
  }
}

void RenderWidget::OnWasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.  If we get swapped back in, we will call
  // AddRefProcess in SetSwappedOut.
  if (is_swapped_out_)
    RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

void RenderWidget::OnUpdateRectAck() {
  TRACE_EVENT0("renderer", "RenderWidget::OnUpdateRectAck");
  DCHECK(update_reply_pending());
  update_reply_pending_ = false;

  // If we sent an UpdateRect message with a zero-sized bitmap, then we should
  // have no current paint buffer.
  if (current_paint_buf_) {
    RenderProcess::current()->ReleaseTransportDIB(current_paint_buf_);
    current_paint_buf_ = NULL;
  }

  // If swapbuffers is still pending, then defer the update until the
  // swapbuffers occurs.
  if (num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending) {
    TRACE_EVENT0("renderer", "EarlyOut_SwapStillPending");
    return;
  }

  // Notify subclasses.
  if (!is_accelerated_compositing_active_)
    DidFlushPaint();

  // Continue painting if necessary...
  DoDeferredUpdateAndSendInputAck();
}

bool RenderWidget::SupportsAsynchronousSwapBuffers()
{
  return false;
}

void RenderWidget::OnSwapBuffersAborted()
{
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersAborted");
  num_swapbuffers_complete_pending_ = 0;
  using_asynchronous_swapbuffers_ = false;
  // Schedule another frame so the compositor learns about it.
  scheduleComposite();
}

void RenderWidget::OnSwapBuffersPosted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersPosted");
  if (using_asynchronous_swapbuffers_)
    num_swapbuffers_complete_pending_++;
}

void RenderWidget::OnSwapBuffersComplete() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersComplete");
  // When compositing deactivates, we reset the swapbuffers pending count.  The
  // swapbuffers acks may still arrive, however.
  if (num_swapbuffers_complete_pending_ == 0) {
    TRACE_EVENT0("renderer", "EarlyOut_ZeroSwapbuffersPending");
    return;
  }
  num_swapbuffers_complete_pending_--;

  // If update reply is still pending, then defer the update until that reply
  // occurs.
  if (update_reply_pending_){
    TRACE_EVENT0("renderer", "EarlyOut_UpdateReplyPending");
    return;
  }

  // If we are not accelerated rendering, then this is a stale swapbuffers from
  // when we were previously rendering. However, if an invalidation task is not
  // posted, there may be software rendering work pending. In that case, don't
  // early out.
  if (!is_accelerated_compositing_active_ && invalidation_task_posted_) {
    TRACE_EVENT0("renderer", "EarlyOut_AcceleratedCompositingOff");
    return;
  }

  // Continue painting if necessary...
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::OnHandleInputEvent(const IPC::Message& message) {
  TRACE_EVENT0("renderer", "RenderWidget::OnHandleInputEvent");
  void* iter = NULL;

  const char* data;
  int data_length;
  handling_input_event_ = true;
  if (!message.ReadData(&iter, &data, &data_length)) {
    handling_input_event_ = false;
    return;
  }

  const WebInputEvent* input_event =
      reinterpret_cast<const WebInputEvent*>(data);

  bool prevent_default = false;
  if (WebInputEvent::isMouseEventType(input_event->type)) {
    prevent_default = WillHandleMouseEvent(
        *(static_cast<const WebMouseEvent*>(input_event)));
  }

  bool processed = prevent_default;
  if (!processed && webwidget_)
    processed = webwidget_->handleInputEvent(*input_event);

  IPC::Message* response =
      new ViewHostMsg_HandleInputEvent_ACK(routing_id_, input_event->type,
                                           processed);

  if ((input_event->type == WebInputEvent::MouseMove ||
       input_event->type == WebInputEvent::MouseWheel ||
       input_event->type == WebInputEvent::TouchMove) &&
      paint_aggregator_.HasPendingUpdate()) {
    // We want to rate limit the input events in this case, so we'll wait for
    // painting to finish before ACKing this message.
    if (pending_input_event_ack_.get()) {
      // As two different kinds of events could cause us to postpone an ack
      // we send it now, if we have one pending. The Browser should never
      // send us the same kind of event we are delaying the ack for.
      Send(pending_input_event_ack_.release());
    }
    pending_input_event_ack_.reset(response);
  } else {
    Send(response);
  }

  handling_input_event_ = false;

  if (!prevent_default) {
    if (WebInputEvent::isKeyboardEventType(input_event->type))
      DidHandleKeyEvent();
    if (WebInputEvent::isMouseEventType(input_event->type))
      DidHandleMouseEvent(*(static_cast<const WebMouseEvent*>(input_event)));
    if (WebInputEvent::isTouchEventType(input_event->type))
      DidHandleTouchEvent(*(static_cast<const WebTouchEvent*>(input_event)));
  }
}

void RenderWidget::OnMouseCaptureLost() {
  if (webwidget_)
    webwidget_->mouseCaptureLost();
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;
  if (webwidget_)
    webwidget_->setFocus(enable);
}

void RenderWidget::ClearFocus() {
  // We may have got the focus from the browser before this gets processed, in
  // which case we do not want to unfocus ourself.
  if (!has_focus_ && webwidget_)
    webwidget_->setFocus(false);
}

void RenderWidget::PaintRect(const gfx::Rect& rect,
                             const gfx::Point& canvas_origin,
                             skia::PlatformCanvas* canvas) {
  TRACE_EVENT2("renderer", "PaintRect",
               "width", rect.width(), "height", rect.height());
  canvas->save();

  // Bring the canvas into the coordinate system of the paint rect.
  canvas->translate(static_cast<SkScalar>(-canvas_origin.x()),
                    static_cast<SkScalar>(-canvas_origin.y()));

  // If there is a custom background, tile it.
  if (!background_.empty()) {
    SkPaint paint;
    SkShader* shader = SkShader::CreateBitmapShader(background_,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    paint.setShader(shader)->unref();

    // Use kSrc_Mode to handle background_ transparency properly.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    // Canvas could contain multiple update rects. Clip to given rect so that
    // we don't accidentally clear other update rects.
    canvas->save();
    SkRect clip;
    clip.set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
             SkIntToScalar(rect.right()), SkIntToScalar(rect.bottom()));
    canvas->clipRect(clip);
    canvas->drawPaint(paint);
    canvas->restore();
  }

  // First see if this rect is a plugin that can paint itself faster.
  TransportDIB* optimized_dib = NULL;
  gfx::Rect optimized_copy_rect, optimized_copy_location;
  webkit::ppapi::PluginInstance* optimized_instance =
      GetBitmapForOptimizedPluginPaint(rect, &optimized_dib,
                                       &optimized_copy_location,
                                       &optimized_copy_rect);
  if (optimized_instance) {
    // This plugin can be optimize-painted and we can just ask it to paint
    // itself. We don't actually need the TransportDIB in this case.
    //
    // This is an optimization for PPAPI plugins that know they're on top of
    // the page content. If this rect is inside such a plugin, we can save some
    // time and avoid re-rendering the page content which we know will be
    // covered by the plugin later (this time can be significant, especially
    // for a playing movie that is invalidating a lot).
    //
    // In the plugin movie case, hopefully the similar call to
    // GetBitmapForOptimizedPluginPaint in DoDeferredUpdate handles the
    // painting, because that avoids copying the plugin image to a different
    // paint rect. Unfortunately, if anything on the page is animating other
    // than the movie, it break this optimization since the union of the
    // invalid regions will be larger than the plugin.
    //
    // This code optimizes that case, where we can still avoid painting in
    // WebKit and filling the background (which can be slow) and just painting
    // the plugin. Unlike the DoDeferredUpdate case, an extra copy is still
    // required.
    optimized_instance->Paint(webkit_glue::ToWebCanvas(canvas),
                              optimized_copy_location, rect);
  } else {
    // Normal painting case.
    webwidget_->paint(webkit_glue::ToWebCanvas(canvas), rect);

    // Flush to underlying bitmap.  TODO(darin): is this needed?
    skia::GetTopDevice(*canvas)->accessBitmap(false);
  }

  PaintDebugBorder(rect, canvas);
  canvas->restore();
}

void RenderWidget::PaintDebugBorder(const gfx::Rect& rect,
                                    skia::PlatformCanvas* canvas) {
  static bool kPaintBorder =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowPaintRects);
  if (!kPaintBorder)
    return;

  // Cycle through these colors to help distinguish new paint rects.
  const SkColor colors[] = {
    SkColorSetARGB(0x3F, 0xFF, 0, 0),
    SkColorSetARGB(0x3F, 0xFF, 0, 0xFF),
    SkColorSetARGB(0x3F, 0, 0, 0xFF),
  };
  static int color_selector = 0;

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(colors[color_selector++ % arraysize(colors)]);
  paint.setStrokeWidth(1);

  SkIRect irect;
  irect.set(rect.x(), rect.y(), rect.right() - 1, rect.bottom() - 1);
  canvas->drawIRect(irect, paint);
}

void RenderWidget::AnimationCallback() {
  TRACE_EVENT0("renderer", "RenderWidget::AnimationCallback");
  animation_task_posted_ = false;
  if (!animation_update_pending_) {
    TRACE_EVENT0("renderer", "EarlyOut_NoAnimationUpdatePending");
    return;
  }
  if (!animation_floor_time_.is_null() && IsRenderingVSynced()) {
    // Record when we fired (according to base::Time::Now()) relative to when
    // we posted the task to quantify how much the base::Time/base::TimeTicks
    // skew is affecting animations.
    base::TimeDelta animation_callback_delay = base::Time::Now() -
        (animation_floor_time_ - base::TimeDelta::FromMilliseconds(16));
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.AnimationCallbackDelayTime",
                               animation_callback_delay,
                               base::TimeDelta::FromMilliseconds(0),
                               base::TimeDelta::FromMilliseconds(30),
                               25);
  }
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::AnimateIfNeeded() {
  if (!animation_update_pending_)
    return;

  // Target 60FPS if vsync is on. Go as fast as we can if vsync is off.
  int animationInterval = IsRenderingVSynced() ? 16 : 0;

  base::Time now = base::Time::Now();
  if (now >= animation_floor_time_) {
    TRACE_EVENT0("renderer", "RenderWidget::AnimateIfNeeded")
    animation_floor_time_ = now +
        base::TimeDelta::FromMilliseconds(animationInterval);
    // Set a timer to call us back after animationInterval before
    // running animation callbacks so that if a callback requests another
    // we'll be sure to run it at the proper time.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&RenderWidget::AnimationCallback, this),
        animationInterval);
    animation_task_posted_ = true;
    animation_update_pending_ = false;
    webwidget_->animate(0.0);
    return;
  }
  TRACE_EVENT0("renderer", "EarlyOut_AnimatedTooRecently");
  if (animation_task_posted_)
    return;
  // This code uses base::Time::Now() to calculate the floor and next fire
  // time because javascript's Date object uses base::Time::Now().  The
  // message loop uses base::TimeTicks, which on windows can have a
  // different granularity than base::Time.
  // The upshot of all this is that this function might be called before
  // base::Time::Now() has advanced past the animation_floor_time_.  To
  // avoid exposing this delay to javascript, we keep posting delayed
  // tasks until base::Time::Now() has advanced far enough.
  int64 delay = (animation_floor_time_ - now).InMillisecondsRoundedUp();
  animation_task_posted_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&RenderWidget::AnimationCallback, this), delay);
}

bool RenderWidget::IsRenderingVSynced() {
  // TODO(nduca): Forcing a driver to disable vsync (e.g. in a control panel) is
  // not caught by this check. This will lead to artificially low frame rates
  // for people who force vsync off at a driver level and expect Chrome to speed
  // up.
  return !has_disable_gpu_vsync_switch_;
}

void RenderWidget::InvalidationCallback() {
  TRACE_EVENT0("renderer", "RenderWidget::InvalidationCallback");
  invalidation_task_posted_ = false;
  DoDeferredUpdateAndSendInputAck();
}

void RenderWidget::DoDeferredUpdateAndSendInputAck() {
  DoDeferredUpdate();

  if (pending_input_event_ack_.get())
    Send(pending_input_event_ack_.release());
}

void RenderWidget::DoDeferredUpdate() {
  TRACE_EVENT0("renderer", "RenderWidget::DoDeferredUpdate");

  if (!webwidget_)
    return;
  if (update_reply_pending()) {
    TRACE_EVENT0("renderer", "EarlyOut_UpdateReplyPending");
    return;
  }
  if (is_accelerated_compositing_active_ &&
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending) {
    TRACE_EVENT0("renderer", "EarlyOut_MaxSwapBuffersPending");
    return;
  }

  // Suppress updating when we are hidden.
  if (is_hidden_ || size_.IsEmpty()) {
    paint_aggregator_.ClearPendingUpdate();
    needs_repainting_on_restore_ = true;
    TRACE_EVENT0("renderer", "EarlyOut_NotVisible");
    return;
  }

  // Tracking of frame rate jitter
  base::TimeTicks frame_begin_ticks = base::TimeTicks::Now();
  AnimateIfNeeded();

  // Layout may generate more invalidation.  It may also enable the
  // GPU acceleration, so make sure to run layout before we send the
  // GpuRenderingActivated message.
  webwidget_->layout();

  // Suppress painting if nothing is dirty.  This has to be done after updating
  // animations running layout as these may generate further invalidations.
  if (!paint_aggregator_.HasPendingUpdate()) {
    TRACE_EVENT0("renderer", "EarlyOut_NoPendingUpdate");
    return;
  }

  if (!last_do_deferred_update_time_.is_null()) {
    base::TimeDelta delay = frame_begin_ticks - last_do_deferred_update_time_;
    if(is_accelerated_compositing_active_)
      UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.AccelDoDeferredUpdateDelay",
                                 delay,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMilliseconds(60),
                                 30);
    else
      UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.SoftwareDoDeferredUpdateDelay",
                                 delay,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMilliseconds(60),
                                 30);

    // Calculate filtered time per frame:
    float frame_time_elapsed = static_cast<float>(delay.InSecondsF());
    filtered_time_per_frame_ =
        0.9f * filtered_time_per_frame_ + 0.1f * frame_time_elapsed;
  }
  last_do_deferred_update_time_ = frame_begin_ticks;

  // OK, save the pending update to a local since painting may cause more
  // invalidation.  Some WebCore rendering objects only layout when painted.
  PaintAggregator::PendingUpdate update;
  paint_aggregator_.PopPendingUpdate(&update);

  gfx::Rect scroll_damage = update.GetScrollDamage();
  gfx::Rect bounds = update.GetPaintBounds().Union(scroll_damage);

  // Compositing the page may disable accelerated compositing.
  bool accelerated_compositing_was_active = is_accelerated_compositing_active_;

  // A plugin may be able to do an optimized paint. First check this, in which
  // case we can skip all of the bitmap generation and regular paint code.
  // This optimization allows PPAPI plugins that declare themselves on top of
  // the page (like a traditional windowed plugin) to be able to animate (think
  // movie playing) without repeatedly re-painting the page underneath, or
  // copying the plugin backing store (since we can send the plugin's backing
  // store directly to the browser).
  //
  // This optimization only works when the entire invalid region is contained
  // within the plugin. There is a related optimization in PaintRect for the
  // case where there may be multiple invalid regions.
  TransportDIB::Id dib_id = TransportDIB::Id();
  TransportDIB* dib = NULL;
  std::vector<gfx::Rect> copy_rects;
  gfx::Rect optimized_copy_rect, optimized_copy_location;
  if (update.scroll_rect.IsEmpty() &&
      !is_accelerated_compositing_active_ &&
      GetBitmapForOptimizedPluginPaint(bounds, &dib, &optimized_copy_location,
                                       &optimized_copy_rect)) {
    // Only update the part of the plugin that actually changed.
    optimized_copy_rect = optimized_copy_rect.Intersect(bounds);
    bounds = optimized_copy_location;
    copy_rects.push_back(optimized_copy_rect);
    dib_id = dib->id();
  } else if (!is_accelerated_compositing_active_) {
    // Compute a buffer for painting and cache it.
    scoped_ptr<skia::PlatformCanvas> canvas(
        RenderProcess::current()->GetDrawingCanvas(&current_paint_buf_,
                                                   bounds));
    if (!canvas.get()) {
      NOTREACHED();
      return;
    }

    // We may get back a smaller canvas than we asked for.
    // TODO(darin): This seems like it could cause painting problems!
    DCHECK_EQ(bounds.width(), canvas->getDevice()->width());
    DCHECK_EQ(bounds.height(), canvas->getDevice()->height());
    bounds.set_width(canvas->getDevice()->width());
    bounds.set_height(canvas->getDevice()->height());

    HISTOGRAM_COUNTS_100("MPArch.RW_PaintRectCount", update.paint_rects.size());

    // The scroll damage is just another rectangle to paint and copy.
    copy_rects.swap(update.paint_rects);
    if (!scroll_damage.IsEmpty())
      copy_rects.push_back(scroll_damage);

    for (size_t i = 0; i < copy_rects.size(); ++i)
      PaintRect(copy_rects[i], bounds.origin(), canvas.get());

    dib_id = current_paint_buf_->id();
  } else {  // Accelerated compositing path
    // Begin painting.
    webwidget_->composite(false);
  }

  // sending an ack to browser process that the paint is complete...
  ViewHostMsg_UpdateRect_Params params;
  params.bitmap = dib_id;
  params.bitmap_rect = bounds;
  params.dx = update.scroll_delta.x();
  params.dy = update.scroll_delta.y();
  if (accelerated_compositing_was_active) {
    // If painting is done via the gpu process then we clear out all damage
    // rects to save the browser process from doing unecessary work.
    params.scroll_rect = gfx::Rect();
    params.copy_rects.clear();
  } else {
    params.scroll_rect = update.scroll_rect;
    params.copy_rects.swap(copy_rects);  // TODO(darin): clip to bounds?
  }
  params.view_size = size_;
  params.resizer_rect = resizer_rect_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;
  params.scroll_offset = GetScrollOffset();

  update_reply_pending_ = true;
  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;

  UpdateTextInputState();
  UpdateSelectionBounds();

  // Let derived classes know we've painted.
  DidInitiatePaint();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::didInvalidateRect(const WebRect& rect) {
  // The invalidated rect might be outside the bounds of the view.
  gfx::Rect view_rect(size_);
  gfx::Rect damaged_rect = view_rect.Intersect(rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.InvalidateRect(damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (invalidation_task_posted_)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending() ||
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending)
    return;

  // When GPU rendering, combine pending animations and invalidations into
  // a single update.
  if (is_accelerated_compositing_active_ && animation_task_posted_)
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  invalidation_task_posted_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::InvalidationCallback, this));
}

void RenderWidget::didScrollRect(int dx, int dy, const WebRect& clip_rect) {
  // Drop scrolls on the floor when we are in compositing mode.
  // TODO(nduca): stop WebViewImpl from sending scrolls in the first place.
  if (is_accelerated_compositing_active_)
    return;

  // The scrolled rect might be outside the bounds of the view.
  gfx::Rect view_rect(size_);
  gfx::Rect damaged_rect = view_rect.Intersect(clip_rect);
  if (damaged_rect.IsEmpty())
    return;

  paint_aggregator_.ScrollRect(dx, dy, damaged_rect);

  // We may not need to schedule another call to DoDeferredUpdate.
  if (invalidation_task_posted_)
    return;
  if (!paint_aggregator_.HasPendingUpdate())
    return;
  if (update_reply_pending() ||
      num_swapbuffers_complete_pending_ >= kMaxSwapBuffersPending)
    return;

  // When GPU rendering, combine pending animations and invalidations into
  // a single update.
  if (is_accelerated_compositing_active_ && animation_task_posted_)
    return;

  // Perform updating asynchronously.  This serves two purposes:
  // 1) Ensures that we call WebView::Paint without a bunch of other junk
  //    on the call stack.
  // 2) Allows us to collect more damage rects before painting to help coalesce
  //    the work that we will need to do.
  invalidation_task_posted_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::InvalidationCallback, this));
}

void RenderWidget::didActivateCompositor(int compositor_identifier) {
  TRACE_EVENT0("gpu", "RenderWidget::didActivateCompositor");

  CompositorThread* compositor_thread =
      RenderThreadImpl::current()->compositor_thread();
  if (compositor_thread)
    compositor_thread->AddCompositor(routing_id_, compositor_identifier);

  is_accelerated_compositing_active_ = true;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));

  // Note: asynchronous swapbuffer support currently only matters if
  // compositing scheduling happens on the RenderWidget.
  using_asynchronous_swapbuffers_ = SupportsAsynchronousSwapBuffers();
}

void RenderWidget::didDeactivateCompositor() {
  TRACE_EVENT0("gpu", "RenderWidget::didDeactivateCompositor");

  is_accelerated_compositing_active_ = false;
  Send(new ViewHostMsg_DidActivateAcceleratedCompositing(
      routing_id_, is_accelerated_compositing_active_));

  if (using_asynchronous_swapbuffers_)
    using_asynchronous_swapbuffers_ = false;
}

void RenderWidget::didCommitAndDrawCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::didCommitAndDrawCompositorFrame");
  // Notify subclasses.
  DidFlushPaint();
}

void RenderWidget::didCompleteSwapBuffers() {
  if (update_reply_pending())
    return;

  if (!next_paint_flags_ && !plugin_window_moves_.size())
    return;

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.resizer_rect = resizer_rect_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;
  params.scroll_offset = GetScrollOffset();
  update_reply_pending_ = true;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
}

void RenderWidget::scheduleComposite() {
  if (WebWidgetHandlesCompositorScheduling())
    webwidget_->composite(false);
  else {
    // TODO(nduca): replace with something a little less hacky.  The reason this
    // hack is still used is because the Invalidate-DoDeferredUpdate loop
    // contains a lot of host-renderer synchronization logic that is still
    // important for the accelerated compositing case. The option of simply
    // duplicating all that code is less desirable than "faking out" the
    // invalidation path using a magical damage rect.
    didInvalidateRect(WebRect(0, 0, 1, 1));
  }
}

void RenderWidget::scheduleAnimation() {
  TRACE_EVENT0("gpu", "RenderWidget::scheduleAnimation");
  if (!animation_update_pending_) {
    animation_update_pending_ = true;
    if (!animation_task_posted_) {
      animation_task_posted_ = true;
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&RenderWidget::AnimationCallback, this));
    }
  }
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor(cursor_info);

  // Only send a SetCursor message if we need to make a change.
  if (!current_cursor_.IsEqual(cursor)) {
    current_cursor_ = cursor;
    Send(new ViewHostMsg_SetCursor(routing_id_, cursor));
  }
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a blocked popup or as a new tab).
//
void RenderWidget::show(WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;

  did_show_ = true;
  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_pos_));
  SetPendingWindowRect(initial_pos_);
}

void RenderWidget::didFocus() {
}

void RenderWidget::didBlur() {
}

void RenderWidget::DoDeferredClose() {
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::closeWidgetSoon() {
  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.

  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::DoDeferredClose, this));
}

void RenderWidget::Close() {
  if (webwidget_) {
    webwidget_->close();
    webwidget_ = NULL;
  }
}

WebRect RenderWidget::windowRect() {
  if (pending_window_rect_count_)
    return pending_window_rect_;

  gfx::Rect rect;
  Send(new ViewHostMsg_GetWindowRect(routing_id_, host_window_, &rect));
  return rect;
}

void RenderWidget::setToolTipText(const WebKit::WebString& text,
                                  WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, text, hint));
}

void RenderWidget::setWindowRect(const WebRect& pos) {
  if (did_show_) {
    Send(new ViewHostMsg_RequestMove(routing_id_, pos));
    SetPendingWindowRect(pos);
  } else {
    initial_pos_ = pos;
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;
}

WebRect RenderWidget::rootWindowRect() {
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    return pending_window_rect_;
  }

  gfx::Rect rect;
  Send(new ViewHostMsg_GetRootWindowRect(routing_id_, host_window_, &rect));
  return rect;
}

WebRect RenderWidget::windowResizerRect() {
  return resizer_rect_;
}

void RenderWidget::OnSetInputMethodActive(bool is_active) {
  // To prevent this renderer process from sending unnecessary IPC messages to
  // a browser process, we permit the renderer process to send IPC messages
  // only during the input method attached to the browser process is active.
  input_method_is_active_ = is_active;
}

void RenderWidget::OnImeSetComposition(
    const string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    int selection_start, int selection_end) {
  if (!webwidget_)
    return;
  if (webwidget_->setComposition(
      text, WebVector<WebCompositionUnderline>(underlines),
      selection_start, selection_end)) {
    // Setting the IME composition was successful. Send the new composition
    // range to the browser.
    ui::Range range(ui::Range::InvalidRange());
    size_t location, length;
    if (webwidget_->compositionRange(&location, &length)) {
      range.set_start(location);
      range.set_end(location + length);
    }
    // The IME was cancelled via the Esc key, so just send back the caret.
    else if (webwidget_->caretOrSelectionRange(&location, &length)) {
      range.set_start(location);
      range.set_end(location + length);
    }
    Send(new ViewHostMsg_ImeCompositionRangeChanged(routing_id(), range));
  } else {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    Send(new ViewHostMsg_ImeCancelComposition(routing_id()));

    // Send an updated IME range with just the caret range.
    ui::Range range(ui::Range::InvalidRange());
    size_t location, length;
    if (webwidget_->caretOrSelectionRange(&location, &length)) {
      range.set_start(location);
      range.set_end(location + length);
    }
    Send(new ViewHostMsg_ImeCompositionRangeChanged(routing_id(), range));
  }
}

void RenderWidget::OnImeConfirmComposition(
    const string16& text, const ui::Range& replacement_range) {
  if (webwidget_) {
    handling_input_event_ = true;
    webwidget_->confirmComposition(text);
    handling_input_event_ = false;
  }
  // Send an updated IME range with just the caret range.
  ui::Range range(ui::Range::InvalidRange());
  size_t location, length;
  if (webwidget_->caretOrSelectionRange(&location, &length)) {
    range.set_start(location);
    range.set_end(location + length);
  }
  Send(new ViewHostMsg_ImeCompositionRangeChanged(routing_id(), range));
}

// This message causes the renderer to render an image of the
// desired_size, regardless of whether the tab is hidden or not.
void RenderWidget::OnMsgPaintAtSize(const TransportDIB::Handle& dib_handle,
                                    int tag,
                                    const gfx::Size& page_size,
                                    const gfx::Size& desired_size) {
  if (!webwidget_ || !TransportDIB::is_valid_handle(dib_handle)) {
    if (TransportDIB::is_valid_handle(dib_handle)) {
      // Close our unused handle.
#if defined(OS_WIN)
      ::CloseHandle(dib_handle);
#elif defined(OS_MACOSX)
      base::SharedMemory::CloseHandle(dib_handle);
#endif
    }
    return;
  }

  if (page_size.IsEmpty() || desired_size.IsEmpty()) {
    // If one of these is empty, then we just return the dib we were
    // given, to avoid leaking it.
    Send(new ViewHostMsg_PaintAtSize_ACK(routing_id_, tag, desired_size));
    return;
  }

  // Map the given DIB ID into this process, and unmap it at the end
  // of this function.
  scoped_ptr<TransportDIB> paint_at_size_buffer(
      TransportDIB::CreateWithHandle(dib_handle));

  gfx::Size canvas_size = page_size;
  float x_scale = static_cast<float>(desired_size.width()) /
                  static_cast<float>(canvas_size.width());
  float y_scale = static_cast<float>(desired_size.height()) /
                  static_cast<float>(canvas_size.height());

  gfx::Rect orig_bounds(canvas_size);
  canvas_size.set_width(static_cast<int>(canvas_size.width() * x_scale));
  canvas_size.set_height(static_cast<int>(canvas_size.height() * y_scale));
  gfx::Rect bounds(canvas_size);

  scoped_ptr<skia::PlatformCanvas> canvas(
      paint_at_size_buffer->GetPlatformCanvas(canvas_size.width(),
                                              canvas_size.height()));
  if (!canvas.get()) {
    NOTREACHED();
    return;
  }

  // Reset bounds to what we actually received, but they should be the
  // same.
  DCHECK_EQ(bounds.width(), canvas->getDevice()->width());
  DCHECK_EQ(bounds.height(), canvas->getDevice()->height());
  bounds.set_width(canvas->getDevice()->width());
  bounds.set_height(canvas->getDevice()->height());

  canvas->save();
  // Add the scale factor to the canvas, so that we'll get the desired size.
  canvas->scale(SkFloatToScalar(x_scale), SkFloatToScalar(y_scale));

  // Have to make sure we're laid out at the right size before
  // rendering.
  gfx::Size old_size = webwidget_->size();
  webwidget_->resize(page_size);
  webwidget_->layout();

  // Paint the entire thing (using original bounds, not scaled bounds).
  PaintRect(orig_bounds, orig_bounds.origin(), canvas.get());
  canvas->restore();

  // Return the widget to its previous size.
  webwidget_->resize(old_size);

  Send(new ViewHostMsg_PaintAtSize_ACK(routing_id_, tag, bounds.size()));
}

void RenderWidget::OnMsgRepaint(const gfx::Size& size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  set_next_paint_is_repaint_ack();
  if (is_accelerated_compositing_active_) {
    scheduleComposite();
  } else {
    gfx::Rect repaint_rect(size_to_paint.width(), size_to_paint.height());
    didInvalidateRect(repaint_rect);
  }
}

void RenderWidget::OnSetTextDirection(WebTextDirection direction) {
  if (!webwidget_)
    return;
  webwidget_->setTextDirection(direction);
}

webkit::ppapi::PluginInstance* RenderWidget::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip) {
  // Bare RenderWidgets don't support optimized plugin painting.
  return NULL;
}

gfx::Point RenderWidget::GetScrollOffset() {
  // Bare RenderWidgets don't support scroll offset.
  return gfx::Point(0, 0);
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it.
  is_hidden_ = hidden;
  if (is_hidden_)
    RenderThread::Get()->WidgetHidden();
  else
    RenderThread::Get()->WidgetRestored();
}

void RenderWidget::WillToggleFullscreen() {
#ifdef WEBKIT_HAS_NEW_FULLSCREEN_API
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->willExitFullScreen();
  } else {
    webwidget_->willEnterFullScreen();
  }
#endif
}

void RenderWidget::DidToggleFullscreen() {
#ifdef WEBKIT_HAS_NEW_FULLSCREEN_API
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->didEnterFullScreen();
  } else {
    webwidget_->didExitFullScreen();
  }
#endif
}

void RenderWidget::SetBackground(const SkBitmap& background) {
  background_ = background;

  // Generate a full repaint.
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_resize_ack(next_paint_flags_);
}

bool RenderWidget::next_paint_is_restore_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_restore_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_restore_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESTORE_ACK;
}

void RenderWidget::set_next_paint_is_repaint_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK;
}

void RenderWidget::UpdateTextInputState() {
  if (!input_method_is_active_)
    return;

  ui::TextInputType new_type = GetTextInputType();
  bool new_can_compose_inline = CanComposeInline();
  // Only sends text input type and compose inline to the browser process if
  // they are changed.
  if (text_input_type_ != new_type ||
      can_compose_inline_ != new_can_compose_inline) {
    text_input_type_ = new_type;
    can_compose_inline_ = new_can_compose_inline;
    Send(new ViewHostMsg_TextInputStateChanged(
        routing_id(), new_type, new_can_compose_inline));
  }
}

void RenderWidget::GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) {
  WebRect start_webrect;
  WebRect end_webrect;
  webwidget_->selectionBounds(start_webrect, end_webrect);
  *start = start_webrect;
  *end = end_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  if (!webwidget_)
    return;

  gfx::Rect start_rect;
  gfx::Rect end_rect;
  GetSelectionBounds(&start_rect, &end_rect);
  if (selection_start_rect_ == start_rect && selection_end_rect_ == end_rect)
    return;

  selection_start_rect_ = start_rect;
  selection_end_rect_ = end_rect;
  Send(new ViewHostMsg_SelectionBoundsChanged(
      routing_id_, selection_start_rect_, selection_end_rect_));
}

// Check WebKit::WebTextInputType and ui::TextInputType is kept in sync.
COMPILE_ASSERT(int(WebKit::WebTextInputTypeNone) == \
               int(ui::TEXT_INPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeText) == \
               int(ui::TEXT_INPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypePassword) == \
               int(ui::TEXT_INPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeSearch) == \
               int(ui::TEXT_INPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeEmail) == \
               int(ui::TEXT_INPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeNumber) == \
               int(ui::TEXT_INPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeTelephone) == \
               int(ui::TEXT_INPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeURL) == \
               int(ui::TEXT_INPUT_TYPE_URL), mismatching_enums);

ui::TextInputType RenderWidget::GetTextInputType() {
  if (webwidget_) {
    int type = webwidget_->textInputType();
    // Check the type is in the range representable by ui::TextInputType.
    DCHECK(type <= ui::TEXT_INPUT_TYPE_URL) <<
      "WebKit::WebTextInputType and ui::TextInputType not synchronized";
    return static_cast<ui::TextInputType>(type);
  }
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool RenderWidget::CanComposeInline() {
  return true;
}

WebScreenInfo RenderWidget::screenInfo() {
  WebScreenInfo results;
  Send(new ViewHostMsg_GetScreenInfo(routing_id_, host_window_, &results));
  return results;
}

void RenderWidget::resetInputMethod() {
  if (!input_method_is_active_)
    return;

  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (webwidget_->confirmComposition())
      Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
  }

  // Send an updated IME range with the current caret rect.
  ui::Range range(ui::Range::InvalidRange());
  size_t location, length;
  if (webwidget_->caretOrSelectionRange(&location, &length)) {
    range.set_start(location);
    range.set_end(location + length);
  }
  Send(new ViewHostMsg_ImeCompositionRangeChanged(routing_id(), range));
}

void RenderWidget::SchedulePluginMove(
    const webkit::npapi::WebPluginGeometry& move) {
  size_t i = 0;
  for (; i < plugin_window_moves_.size(); ++i) {
    if (plugin_window_moves_[i].window == move.window) {
      if (move.rects_valid) {
        plugin_window_moves_[i] = move;
      } else {
        plugin_window_moves_[i].visible = move.visible;
      }
      break;
    }
  }

  if (i == plugin_window_moves_.size())
    plugin_window_moves_.push_back(move);
}

void RenderWidget::CleanupWindowInPluginMoves(gfx::PluginWindowHandle window) {
  for (WebPluginGeometryVector::iterator i = plugin_window_moves_.begin();
       i != plugin_window_moves_.end(); ++i) {
    if (i->window == window) {
      plugin_window_moves_.erase(i);
      break;
    }
  }
}

bool RenderWidget::WillHandleMouseEvent(const WebKit::WebMouseEvent& event) {
  return false;
}

bool RenderWidget::WebWidgetHandlesCompositorScheduling() const {
  return false;
}
