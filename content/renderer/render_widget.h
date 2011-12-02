// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "content/renderer/paint_aggregator.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidgetClient.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/transport_dib.h"
#include "webkit/glue/webcursor.h"

namespace IPC {
class SyncMessage;
}

namespace WebKit {
class WebMouseEvent;
class WebTouchEvent;
class WebWidget;
}

namespace gfx {
class Point;
}

namespace skia {
class PlatformCanvas;
}

namespace ui {
class Range;
}

namespace webkit {
namespace npapi {
struct WebPluginGeometry;
}  // namespace npapi

namespace ppapi {
class PluginInstance;
}  // namespace ppapi
}  // namespace webkit

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
class CONTENT_EXPORT RenderWidget
    : public IPC::Channel::Listener,
      public IPC::Message::Sender,
      NON_EXPORTED_BASE(virtual public WebKit::WebWidgetClient),
      public base::RefCounted<RenderWidget> {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside.
  static RenderWidget* Create(int32 opener_id,
                              WebKit::WebPopupType popup_type);

  // Creates a WebWidget based on the popup type.
  static WebKit::WebWidget* CreateWebWidget(RenderWidget* render_widget);

  // The compositing surface assigned by the RenderWidgetHost
  // (or RenderViewHost). Will be gfx::kNullPluginWindow if not assigned yet,
  // in which case we should not create any GPU command buffers with it.
  // The routing ID assigned by the RenderProcess. Will be MSG_ROUTING_NONE if
  // not yet assigned a view ID, in which case, the process MUST NOT send
  // messages with this ID to the parent.
  int32 routing_id() const {
    return routing_id_;
  }

  // May return NULL when the window is closing.
  WebKit::WebWidget* webwidget() const { return webwidget_; }

  gfx::NativeViewId host_window() const { return host_window_; }
  gfx::Size size() const { return size_; }
  bool has_focus() const { return has_focus_; }
  bool is_fullscreen() const { return is_fullscreen_; }

  // IPC::Channel::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC::Message::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // WebKit::WebWidgetClient
  virtual void didInvalidateRect(const WebKit::WebRect&);
  virtual void didScrollRect(int dx, int dy, const WebKit::WebRect& clipRect);
  virtual void didAutoResize(const WebKit::WebSize& new_size);
  virtual void didActivateCompositor(int compositorIdentifier);
  virtual void didDeactivateCompositor();
  virtual void didCommitAndDrawCompositorFrame();
  virtual void didCompleteSwapBuffers();
  virtual void scheduleComposite();
  virtual void scheduleAnimation();
  virtual void didFocus();
  virtual void didBlur();
  virtual void didChangeCursor(const WebKit::WebCursorInfo&);
  virtual void closeWidgetSoon();
  virtual void show(WebKit::WebNavigationPolicy);
  virtual void runModal() {}
  virtual WebKit::WebRect windowRect();
  virtual void setToolTipText(const WebKit::WebString& text,
                              WebKit::WebTextDirection hint);
  virtual void setWindowRect(const WebKit::WebRect&);
  virtual WebKit::WebRect windowResizerRect();
  virtual WebKit::WebRect rootWindowRect();
  virtual WebKit::WebScreenInfo screenInfo();
  virtual void resetInputMethod();

  // Called when a plugin is moved.  These events are queued up and sent with
  // the next paint or scroll message to the host.
  void SchedulePluginMove(const webkit::npapi::WebPluginGeometry& move);

  // Called when a plugin window has been destroyed, to make sure the currently
  // pending moves don't try to reference it.
  void CleanupWindowInPluginMoves(gfx::PluginWindowHandle window);

  // Close the underlying WebWidget.
  virtual void Close();

  float filtered_time_per_frame() const {
    return filtered_time_per_frame_;
  }

 protected:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;
  // For unit tests.
  friend class RenderWidgetTest;

  explicit RenderWidget(WebKit::WebPopupType popup_type);
  virtual ~RenderWidget();

  // Initializes this view with the given opener.  CompleteInit must be called
  // later.
  void Init(int32 opener_id);

  // Called by Init and subclasses to perform initialization.
  void DoInit(int32 opener_id,
              WebKit::WebWidget* web_widget,
              IPC::SyncMessage* create_widget_message);

  // Finishes creation of a pending view started with Init.
  void CompleteInit(gfx::NativeViewId parent);

  // Sets whether this RenderWidget has been swapped out to be displayed by
  // a RenderWidget in a different process.  If so, no new IPC messages will be
  // sent (only ACKs) and the process is free to exit when there are no other
  // active RenderWidgets.
  void SetSwappedOut(bool is_swapped_out);

  // Paints the given rectangular region of the WebWidget into canvas (a
  // shared memory segment returned by AllocPaintBuf on Windows). The caller
  // must ensure that the given rect fits within the bounds of the WebWidget.
  void PaintRect(const gfx::Rect& rect, const gfx::Point& canvas_origin,
                 skia::PlatformCanvas* canvas);

  // Paints a border at the given rect for debugging purposes.
  void PaintDebugBorder(const gfx::Rect& rect, skia::PlatformCanvas* canvas);

  bool IsRenderingVSynced();
  void AnimationCallback();
  void AnimateIfNeeded();
  void InvalidationCallback();
  void DoDeferredUpdateAndSendInputAck();
  void DoDeferredUpdate();
  void DoDeferredClose();
  void DoDeferredSetWindowRect(const WebKit::WebRect& pos);

  // Set the background of the render widget to a bitmap. The bitmap will be
  // tiled in both directions if it isn't big enough to fill the area. This is
  // mainly intended to be used in conjuction with WebView::SetIsTransparent().
  virtual void SetBackground(const SkBitmap& bitmap);

  // RenderWidget IPC message handlers
  void OnClose();
  void OnCreatingNewAck(gfx::NativeViewId parent);
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Rect& resizer_rect,
                        bool is_fullscreen);
  virtual void OnWasHidden();
  virtual void OnWasRestored(bool needs_repainting);
  virtual void OnWasSwappedOut();
  void OnUpdateRectAck();
  void OnCreateVideoAck(int32 video_id);
  void OnUpdateVideoAck(int32 video_id);
  void OnRequestMoveAck();
  void OnHandleInputEvent(const IPC::Message& message);
  void OnMouseCaptureLost();
  virtual void OnSetFocus(bool enable);
  void OnSetInputMethodActive(bool is_active);
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  virtual void OnImeConfirmComposition(
      const string16& text, const ui::Range& replacement_range);
  void OnMsgPaintAtSize(const TransportDIB::Handle& dib_id,
                        int tag,
                        const gfx::Size& page_size,
                        const gfx::Size& desired_size);
  void OnMsgRepaint(const gfx::Size& size_to_paint);
  void OnSetTextDirection(WebKit::WebTextDirection direction);
  void OnGetFPS();

  // Override point to notify derived classes that a paint has happened.
  // DidInitiatePaint happens when we've generated a new bitmap and sent it to
  // the browser. DidFlushPaint happens once we've received the ACK that the
  // screen has actually been updated.
  virtual void DidInitiatePaint() {}
  virtual void DidFlushPaint() {}

  // Override and return true when the widget is rendered with a graphics
  // context that supports asynchronous swapbuffers. When returning true, the
  // subclass must call OnSwapBuffersPosted() when swap is posted,
  // OnSwapBuffersComplete() when swaps complete, and OnSwapBuffersAborted if
  // the context is lost.
  virtual bool SupportsAsynchronousSwapBuffers();

  // Notifies scheduler that the RenderWidget's subclass has finished or aborted
  // a swap buffers.
  void OnSwapBuffersPosted();
  void OnSwapBuffersComplete();
  void OnSwapBuffersAborted();

  // Detects if a suitable opaque plugin covers the given paint bounds with no
  // compositing necessary.
  //
  // Returns the plugin instance that's the source of the paint if the paint
  // can be handled by just blitting the plugin bitmap. In this case, the
  // location, clipping, and ID of the backing store will be filled into the
  // given output parameters.
  //
  // A return value of null means optimized painting can not be used and we
  // should continue with the normal painting code path.
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip);

  // Gets the scroll offset of this widget, if this widget has a notion of
  // scroll offset.
  virtual gfx::Point GetScrollOffset();

  // Sets the "hidden" state of this widget.  All accesses to is_hidden_ should
  // use this method so that we can properly inform the RenderThread of our
  // state.
  void SetHidden(bool hidden);

  bool is_hidden() const { return is_hidden_; }

  void WillToggleFullscreen();
  void DidToggleFullscreen();

  // True if an UpdateRect_ACK message is pending.
  bool update_reply_pending() const {
    return update_reply_pending_;
  }

  bool next_paint_is_resize_ack() const;
  bool next_paint_is_restore_ack() const;
  void set_next_paint_is_resize_ack();
  void set_next_paint_is_restore_ack();
  void set_next_paint_is_repaint_ack();

  // Checks if the text input state and compose inline mode have been changed.
  // If they are changed, the new value will be sent to the browser process.
  void UpdateTextInputState();

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  // Override point to obtain that the current input method state and caret
  // position.
  virtual ui::TextInputType GetTextInputType();
  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  // Override point to obtain that the current input method state about
  // composition text.
  virtual bool CanComposeInline();

  // Tells the renderer it does not have focus. Used to prevent us from getting
  // the focus on our own when the browser did not focus us.
  void ClearFocus();

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const WebKit::WebRect& r);

  // Called by OnHandleInputEvent() to notify subclasses that a key event was
  // just handled.
  virtual void DidHandleKeyEvent() {}

  // Called by OnHandleInputEvent() to notify subclasses that a mouse event is
  // about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit or trigger DidHandleMouseEvent().
  virtual bool WillHandleMouseEvent(const WebKit::WebMouseEvent& event);

  // Called by OnHandleInputEvent() to notify subclasses that a mouse event was
  // just handled.
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {}

  // Called by OnHandleInputEvent() to notify subclasses that a touch event was
  // just handled.
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event) {}

  // Should return true if the underlying WebWidget is responsible for
  // the scheduling of compositing requests.
  virtual bool WebWidgetHandlesCompositorScheduling() const;

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost. When MSG_ROUTING_NONE, no messages may be sent.
  int32 routing_id_;

  // We are responsible for destroying this object via its Close method.
  WebKit::WebWidget* webwidget_;

  // Set to the ID of the view that initiated creating this view, if any. When
  // the view was initiated by the browser (the common case), this will be
  // MSG_ROUTING_NONE. This is used in determining ownership when opening
  // child tabs. See RenderWidget::createWebViewWithRequest.
  //
  // This ID may refer to an invalid view if that view is closed before this
  // view is.
  int32 opener_id_;

  // The position where this view should be initially shown.
  gfx::Rect initial_pos_;

  // The window we are embedded within.  TODO(darin): kill this.
  gfx::NativeViewId host_window_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;

  // The size of the RenderWidget.
  gfx::Size size_;

  // The TransportDIB that is being used to transfer an image to the browser.
  TransportDIB* current_paint_buf_;

  PaintAggregator paint_aggregator_;

  // The area that must be reserved for drawing the resize corner.
  gfx::Rect resizer_rect_;

  // Flags for the next ViewHostMsg_UpdateRect message.
  int next_paint_flags_;

  // Filtered time per frame based on UpdateRect messages.
  float filtered_time_per_frame_;

  // True if we are expecting an UpdateRect_ACK message (i.e., that a
  // UpdateRect message has been sent).
  bool update_reply_pending_;

  // True if the underlying graphics context supports asynchronous swap.
  // Cached on the RenderWidget because determining support is costly.
  bool using_asynchronous_swapbuffers_;

  // Number of OnSwapBuffersComplete we are expecting. Incremented each time
  // WebWidget::composite has been been performed when the RenderWidget subclass
  // SupportsAsynchronousSwapBuffers. Decremented in OnSwapBuffers. Will block
  // rendering.
  int num_swapbuffers_complete_pending_;

  // When accelerated rendering is on, is the maximum number of swapbuffers that
  // can be outstanding before we start throttling based on
  // OnSwapBuffersComplete callback.
  static const int kMaxSwapBuffersPending = 2;

  // Set to true if we should ignore RenderWidget::Show calls.
  bool did_show_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are in fullscreen mode.
  bool is_fullscreen_;

  // Indicates that we should be repainted when restored.  This flag is set to
  // true if we receive an invalidation / scroll event from webkit while our
  // is_hidden_ flag is set to true.  This is used to force a repaint once we
  // restore to account for the fact that our host would not know about the
  // invalidation / scroll event(s) from webkit while we are hidden.
  bool needs_repainting_on_restore_;

  // Indicates whether we have been focused/unfocused by the browser.
  bool has_focus_;

  // Are we currently handling an input event?
  bool handling_input_event_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // Whether this RenderWidget is currently swapped out, such that the view is
  // being rendered by another process.  If all RenderWidgets in a process are
  // swapped out, the process can exit.
  bool is_swapped_out_;

  // Indicates if an input method is active in the browser process.
  bool input_method_is_active_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_start_rect_;
  gfx::Rect selection_end_rect_;

  // The kind of popup this widget represents, NONE if not a popup.
  WebKit::WebPopupType popup_type_;

  // Holds all the needed plugin window moves for a scroll.
  typedef std::vector<webkit::npapi::WebPluginGeometry> WebPluginGeometryVector;
  WebPluginGeometryVector plugin_window_moves_;

  // A custom background for the widget.
  SkBitmap background_;

  // While we are waiting for the browser to update window sizes,
  // we track the pending size temporarily.
  int pending_window_rect_count_;
  WebKit::WebRect pending_window_rect_;

  scoped_ptr<IPC::Message> pending_input_event_ack_;

  // Set to true if painting to the window is handled by the accelerated
  // compositor.
  bool is_accelerated_compositing_active_;

  base::Time animation_floor_time_;
  bool animation_update_pending_;
  bool animation_task_posted_;
  bool invalidation_task_posted_;

  bool has_disable_gpu_vsync_switch_;
  base::TimeTicks last_do_deferred_update_time_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
