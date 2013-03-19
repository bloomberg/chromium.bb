// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <deque>
#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "cc/debug/rendering_stats.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/paint_aggregator.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidgetClient.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/range/range.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"
#include "ui/surface/transport_dib.h"
#include "webkit/glue/webcursor.h"

struct ViewHostMsg_UpdateRect_Params;
class ViewHostMsg_UpdateRect;

namespace IPC {
class SyncMessage;
}

namespace WebKit {
class WebGestureEvent;
class WebInputEvent;
class WebMouseEvent;
class WebTouchEvent;
struct WebPoint;
struct WebRenderingStatsImpl;
}

namespace cc { class OutputSurface; }

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

namespace content {
struct GpuRenderingStats;
class RenderWidgetCompositor;
class RenderWidgetTest;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      NON_EXPORTED_BASE(virtual public WebKit::WebWidgetClient),
      NON_EXPORTED_BASE(public WebGraphicsContext3DSwapBuffersClient),
      public base::RefCounted<RenderWidget> {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside.
  static RenderWidget* Create(int32 opener_id,
                              WebKit::WebPopupType popup_type,
                              const WebKit::WebScreenInfo& screen_info);

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

  int32 surface_id() const {
    return surface_id_;
  }

  // May return NULL when the window is closing.
  WebKit::WebWidget* webwidget() const { return webwidget_; }

  gfx::Size size() const { return size_; }
  bool has_focus() const { return has_focus_; }
  bool is_fullscreen() const { return is_fullscreen_; }
  bool is_hidden() const { return is_hidden_; }

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // WebKit::WebWidgetClient
  virtual void suppressCompositorScheduling(bool enable);
  virtual void willBeginCompositorFrame();
  virtual void didInvalidateRect(const WebKit::WebRect&);
  virtual void didScrollRect(int dx, int dy,
                             const WebKit::WebRect& clipRect);
  virtual void didAutoResize(const WebKit::WebSize& new_size);
  virtual void didActivateCompositor(int input_handler_identifier);
  virtual void didDeactivateCompositor();
  virtual void initializeLayerTreeView();
  virtual WebKit::WebLayerTreeView* layerTreeView();
  virtual void didBecomeReadyForAdditionalInput();
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
  virtual float deviceScaleFactor();
  virtual void resetInputMethod();
  virtual void didHandleGestureEvent(const WebKit::WebGestureEvent& event,
                                     bool event_cancelled);

  // Called when a plugin is moved.  These events are queued up and sent with
  // the next paint or scroll message to the host.
  void SchedulePluginMove(const webkit::npapi::WebPluginGeometry& move);

  // Called when a plugin window has been destroyed, to make sure the currently
  // pending moves don't try to reference it.
  void CleanupWindowInPluginMoves(gfx::PluginWindowHandle window);

  // Fills in a WebRenderingStatsImpl struct containing information about
  // rendering, e.g. count of frames rendered, time spent painting.
  // This call is relatively expensive in threaded compositing mode,
  // as it blocks on the compositor thread.
  void GetRenderingStats(WebKit::WebRenderingStatsImpl&) const;

  // Fills in a GpuRenderingStats struct containing information about
  // GPU rendering, e.g. count of texture uploads performed, time spent
  // uploading.
  // This call is relatively expensive as it blocks on the GPU process
  bool GetGpuRenderingStats(GpuRenderingStats*) const;

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface();

  // Callback for use with BeginSmoothScroll.
  typedef base::Callback<void()> SmoothScrollCompletionCallback;

  // Directs the host to begin a smooth scroll. This scroll should have the same
  // performance characteristics as a user-initiated scroll. Returns an ID of
  // the scroll gesture. |mouse_event_x| and |mouse_event_y| are expected to be
  // in local DIP coordinates.
  void BeginSmoothScroll(bool scroll_down,
                         const SmoothScrollCompletionCallback& callback,
                         int pixels_to_scroll,
                         int mouse_event_x,
                         int mouse_event_y);

  // Close the underlying WebWidget.
  virtual void Close();

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame();

  float filtered_time_per_frame() const {
    return filtered_time_per_frame_;
  }

  enum ShowIme {
    DO_NOT_SHOW_IME,
    SHOW_IME_IF_NEEDED
  };

  virtual void InstrumentWillBeginFrame() {}
  virtual void InstrumentDidBeginFrame() {}
  virtual void InstrumentDidCancelFrame() {}
  virtual void InstrumentWillComposite() {}

  virtual bool AllowPartialSwap() const;

 protected:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;
  // For unit tests.
  friend class RenderWidgetTest;

  enum ResizeAck {
    SEND_RESIZE_ACK,
    NO_RESIZE_ACK,
  };

  RenderWidget(WebKit::WebPopupType popup_type,
               const WebKit::WebScreenInfo& screen_info,
               bool swapped_out);

  virtual ~RenderWidget();

  // Initializes this view with the given opener.  CompleteInit must be called
  // later.
  bool Init(int32 opener_id);

  // Called by Init and subclasses to perform initialization.
  bool DoInit(int32 opener_id,
              WebKit::WebWidget* web_widget,
              IPC::SyncMessage* create_widget_message);

  // Finishes creation of a pending view started with Init.
  void CompleteInit();

  // Sets whether this RenderWidget has been swapped out to be displayed by
  // a RenderWidget in a different process.  If so, no new IPC messages will be
  // sent (only ACKs) and the process is free to exit when there are no other
  // active RenderWidgets.
  void SetSwappedOut(bool is_swapped_out);

  // Paints the given rectangular region of the WebWidget into canvas (a
  // shared memory segment returned by AllocPaintBuf on Windows). The caller
  // must ensure that the given rect fits within the bounds of the WebWidget.
  void PaintRect(const gfx::Rect& rect, const gfx::Point& canvas_origin,
                 SkCanvas* canvas);

  // Paints a border at the given rect for debugging purposes.
  void PaintDebugBorder(const gfx::Rect& rect, SkCanvas* canvas);

  bool IsRenderingVSynced();
  void AnimationCallback();
  void AnimateIfNeeded();
  void InvalidationCallback();
  void DoDeferredUpdateAndSendInputAck();
  void DoDeferredUpdate();
  void DoDeferredClose();
  void DoDeferredSetWindowRect(const WebKit::WebRect& pos);
  virtual void Composite(base::TimeTicks frame_begin_time);

  // Set the background of the render widget to a bitmap. The bitmap will be
  // tiled in both directions if it isn't big enough to fill the area. This is
  // mainly intended to be used in conjuction with WebView::SetIsTransparent().
  virtual void SetBackground(const SkBitmap& bitmap);

  // Resizes the render widget.
  void Resize(const gfx::Size& new_size,
              const gfx::Size& physical_backing_size,
              const gfx::Rect& resizer_rect,
              bool is_fullscreen,
              ResizeAck resize_ack);

  // RenderWidget IPC message handlers
  void OnClose();
  void OnCreatingNewAck();
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Size& physical_backing_size,
                        const gfx::Rect& resizer_rect,
                        bool is_fullscreen);
  void OnChangeResizeRect(const gfx::Rect& resizer_rect);
  virtual void OnWasHidden();
  virtual void OnWasShown(bool needs_repainting);
  virtual void OnWasSwappedOut();
  void OnUpdateRectAck();
  void OnCreateVideoAck(int32 video_id);
  void OnUpdateVideoAck(int32 video_id);
  void OnRequestMoveAck();
  void OnHandleInputEvent(const WebKit::WebInputEvent* event,
                          bool keyboard_shortcut);
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
  void OnPaintAtSize(const TransportDIB::Handle& dib_id,
                     int tag,
                     const gfx::Size& page_size,
                     const gfx::Size& desired_size);
  void OnRepaint(const gfx::Size& size_to_paint);
  void OnSmoothScrollCompleted(int gesture_id);
  void OnSetTextDirection(WebKit::WebTextDirection direction);
  void OnGetFPS();
  void OnScreenInfoChanged(const WebKit::WebScreenInfo& screen_info);
  void OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                           const gfx::Rect& window_screen_rect);
#if defined(OS_ANDROID)
  void OnImeBatchStateChanged(bool is_begin);
  void OnShowImeIfNeeded();
#endif

  // Notify the compositor about a change in viewport size. This should be
  // used only with auto resize mode WebWidgets, as normal WebWidgets should
  // go through OnResize.
  void AutoResizeCompositor();

  virtual void SetDeviceScaleFactor(float device_scale_factor);

  // Override points to notify derived classes that a paint has happened.
  // WillInitiatePaint happens when we're about to generate a new bitmap and
  // send it to the browser. DidInitiatePaint happens when that has completed,
  // and subsequent rendering won't affect the painted content. DidFlushPaint
  // happens once we've received the ACK that the screen has been updated.
  // For a given paint operation, these overrides will always be called in the
  // order WillInitiatePaint, DidInitiatePaint, DidFlushPaint.
  virtual void WillInitiatePaint() {}
  virtual void DidInitiatePaint() {}
  virtual void DidFlushPaint() {}

  // Override and return true when the widget is rendered with a graphics
  // context that supports asynchronous swapbuffers. When returning true, the
  // subclass must call OnSwapBuffersPosted() when swap is posted,
  // OnSwapBuffersComplete() when swaps complete, and OnSwapBuffersAborted if
  // the context is lost.
  virtual bool SupportsAsynchronousSwapBuffers();
  virtual GURL GetURLForGraphicsContext3D();

  virtual bool ForceCompositingModeEnabled();

  // WebGraphicsContext3DSwapBuffersClient implementation.

  // Called by a GraphicsContext associated with this view when swapbuffers
  // is posted, completes or is aborted.
  virtual void OnViewContextSwapBuffersPosted() OVERRIDE;
  virtual void OnViewContextSwapBuffersComplete() OVERRIDE;
  virtual void OnViewContextSwapBuffersAborted() OVERRIDE;

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
      gfx::Rect* clip,
      float* scale_factor);

  // Gets the scroll offset of this widget, if this widget has a notion of
  // scroll offset.
  virtual gfx::Vector2d GetScrollOffset();

  // Sets the "hidden" state of this widget.  All accesses to is_hidden_ should
  // use this method so that we can properly inform the RenderThread of our
  // state.
  void SetHidden(bool hidden);

  void WillToggleFullscreen();
  void DidToggleFullscreen();

  bool next_paint_is_resize_ack() const;
  bool next_paint_is_restore_ack() const;
  void set_next_paint_is_resize_ack();
  void set_next_paint_is_restore_ack();
  void set_next_paint_is_repaint_ack();

  void set_throttle_input_events(bool throttle_input_events) {
    throttle_input_events_ = throttle_input_events;
  }

  // Checks if the text input state and compose inline mode have been changed.
  // If they are changed, the new value will be sent to the browser process.
  // |show_ime_if_needed| should be SHOW_IME_IF_NEEDED iff the update may cause
  // the ime to be displayed, e.g. after a tap on an input field on mobile.
  void UpdateTextInputState(ShowIme show_ime);

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  virtual void UpdateSelectionBounds();

  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process.
  virtual void UpdateCompositionInfo(
      const ui::Range& range,
      const std::vector<gfx::Rect>& character_bounds);

  // Override point to obtain that the current input method state and caret
  // position.
  virtual ui::TextInputType GetTextInputType();
  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);
  virtual ui::TextInputType WebKitToUiTextInputType(
      WebKit::WebTextInputType type);

  // Override point to obtain that the current composition character bounds.
  // In the case of surrogate pairs, the character is treated as two characters:
  // the bounds for first character is actual one, and the bounds for second
  // character is zero width rectangle.
  virtual void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds);

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(
      const ui::Range& range,
      const std::vector<gfx::Rect>& bounds);

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

  // Called by OnHandleInputEvent() to notify subclasses that a gesture event is
  // about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit.
  virtual bool WillHandleGestureEvent(const WebKit::WebGestureEvent& event);

  // Called by OnHandleInputEvent() to notify subclasses that a mouse event was
  // just handled.
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {}

  // Called by OnHandleInputEvent() to notify subclasses that a touch event was
  // just handled.
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event) {}

  // Check whether the WebWidget has any touch event handlers registered
  // at the given point.
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const;

  // Creates a 3D context associated with this view.
  WebKit::WebGraphicsContext3D* CreateGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes);

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost. When MSG_ROUTING_NONE, no messages may be sent.
  int32 routing_id_;

  int32 surface_id_;

  // We are responsible for destroying this object via its Close method.
  WebKit::WebWidget* webwidget_;

  // This is lazily constructed and must not outlive webwidget_.
  scoped_ptr<RenderWidgetCompositor> compositor_;

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

  bool init_complete_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;

  // The size of the RenderWidget.
  gfx::Size size_;

  // The TransportDIB that is being used to transfer an image to the browser.
  TransportDIB* current_paint_buf_;

  PaintAggregator paint_aggregator_;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  gfx::Size physical_backing_size_;

  // The area that must be reserved for drawing the resize corner.
  gfx::Rect resizer_rect_;

  // Flags for the next ViewHostMsg_UpdateRect message.
  int next_paint_flags_;

  // Filtered time per frame based on UpdateRect messages.
  float filtered_time_per_frame_;

  // True if we are expecting an UpdateRect_ACK message (i.e., that a
  // UpdateRect message has been sent).
  bool update_reply_pending_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_;

  // True if we need to send an UpdateRect message to notify the browser about
  // an already-completed auto-resize.
  bool need_update_rect_for_auto_resize_;

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

  // Are we currently handling an ime event?
  bool handling_ime_event_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // Whether this RenderWidget is currently swapped out, such that the view is
  // being rendered by another process.  If all RenderWidgets in a process are
  // swapped out, the process can exit.
  bool is_swapped_out_;

  // Indicates if an input method is active in the browser process.
  bool input_method_is_active_;

  // Stores information about the current text input.
  WebKit::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  ui::Range composition_range_;

  // The kind of popup this widget represents, NONE if not a popup.
  WebKit::WebPopupType popup_type_;

  // Holds all the needed plugin window moves for a scroll.
  typedef std::vector<webkit::npapi::WebPluginGeometry> WebPluginGeometryVector;
  WebPluginGeometryVector plugin_window_moves_;

  // A custom background for the widget.
  SkBitmap background_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  WebKit::WebRect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect view_screen_rect_;
  gfx::Rect window_screen_rect_;

  scoped_ptr<IPC::Message> pending_input_event_ack_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_;

  // Set to true if painting to the window is handled by the accelerated
  // compositor.
  bool is_accelerated_compositing_active_;

  base::OneShotTimer<RenderWidget> animation_timer_;
  base::Time animation_floor_time_;
  bool animation_update_pending_;
  bool invalidation_task_posted_;

  bool has_disable_gpu_vsync_switch_;
  base::TimeTicks last_do_deferred_update_time_;

  cc::RenderingStats software_stats_;

  // UpdateRect parameters for the current compositing pass. This is used to
  // pass state between DoDeferredUpdate and OnSwapBuffersPosted.
  scoped_ptr<ViewHostMsg_UpdateRect_Params> pending_update_params_;

  // Queue of UpdateRect messages corresponding to a SwapBuffers. We want to
  // delay sending of UpdateRect until the corresponding SwapBuffers has been
  // executed. Since we can have several in flight, we need to keep them in a
  // queue. Note: some SwapBuffers may not correspond to an update, in which
  // case NULL is added to the queue.
  std::deque<ViewHostMsg_UpdateRect*> updates_pending_swap_;

  // Properties of the screen hosting this RenderWidget instance.
  WebKit::WebScreenInfo screen_info_;

  // The device scale factor. This value is computed from the DPI entries in
  // |screen_info_| on some platforms, and defaults to 1 on other platforms.
  float device_scale_factor_;

  // Specifies whether input event throttling is enabled for this widget.
  bool throttle_input_events_;

  // State associated with the BeginSmoothScroll synthetic scrolling function.
  int next_smooth_scroll_gesture_id_;
  typedef std::map<int, SmoothScrollCompletionCallback>
      PendingSmoothScrollGestureMap;
  PendingSmoothScrollGestureMap pending_smooth_scroll_gestures_;

  // Specified whether the compositor will run in its own thread.
  bool is_threaded_compositing_enabled_;

  base::WeakPtrFactory<RenderWidget> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
