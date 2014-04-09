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
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/renderer/paint_aggregator.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebTextInputInfo.h"
#include "third_party/WebKit/public/web/WebTouchAction.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"
#include "ui/surface/transport_dib.h"

struct ViewHostMsg_UpdateRect_Params;
struct ViewMsg_Resize_Params;
class ViewHostMsg_UpdateRect;

namespace IPC {
class SyncMessage;
}

namespace blink {
struct WebDeviceEmulationParams;
class WebGestureEvent;
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebTouchEvent;
}

namespace cc { class OutputSurface; }

namespace gfx {
class Range;
}

namespace content {
class ExternalPopupMenu;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderWidgetCompositor;
class RenderWidgetTest;
class ResizingModeSelector;
struct ContextMenuParams;
struct WebPluginGeometry;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      NON_EXPORTED_BASE(virtual public blink::WebWidgetClient),
      public base::RefCounted<RenderWidget> {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside.
  static RenderWidget* Create(int32 opener_id,
                              blink::WebPopupType popup_type,
                              const blink::WebScreenInfo& screen_info);

  // Creates a WebWidget based on the popup type.
  static blink::WebWidget* CreateWebWidget(RenderWidget* render_widget);

  int32 routing_id() const { return routing_id_; }
  int32 surface_id() const { return surface_id_; }
  blink::WebWidget* webwidget() const { return webwidget_; }
  gfx::Size size() const { return size_; }
  float filtered_time_per_frame() const { return filtered_time_per_frame_; }
  bool has_focus() const { return has_focus_; }
  bool is_fullscreen() const { return is_fullscreen_; }
  bool is_hidden() const { return is_hidden_; }
  bool handling_input_event() const { return handling_input_event_; }
  // Temporary for debugging purposes...
  bool closing() const { return closing_; }
  bool is_swapped_out() { return is_swapped_out_; }
  ui::MenuSourceType context_menu_source_type() {
    return context_menu_source_type_; }
  gfx::Point touch_editing_context_menu_location() {
    return touch_editing_context_menu_location_;
  }

  // Functions to track out-of-process frames for special notifications.
  void RegisterSwappedOutChildFrame(RenderFrameImpl* frame);
  void UnregisterSwappedOutChildFrame(RenderFrameImpl* frame);

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // blink::WebWidgetClient
  virtual void suppressCompositorScheduling(bool enable);
  virtual void willBeginCompositorFrame();
  virtual void didInvalidateRect(const blink::WebRect&);
  virtual void didScrollRect(int dx, int dy,
                             const blink::WebRect& clipRect);
  virtual void didAutoResize(const blink::WebSize& new_size);
  // FIXME: To be removed as soon as chromium and blink side changes land
  // didActivateCompositor with parameters is still kept in order to land
  // these changes s-chromium - https://codereview.chromium.org/137893025/.
  // s-blink - https://codereview.chromium.org/138523003/
  virtual void didActivateCompositor(int input_handler_identifier);
  virtual void didActivateCompositor() OVERRIDE;
  virtual void didDeactivateCompositor();
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();
  virtual void didBecomeReadyForAdditionalInput();
  virtual void didCommitAndDrawCompositorFrame();
  virtual void didCompleteSwapBuffers();
  virtual void scheduleComposite();
  virtual void scheduleAnimation();
  virtual void didFocus();
  virtual void didBlur();
  virtual void didChangeCursor(const blink::WebCursorInfo&);
  virtual void closeWidgetSoon();
  virtual void show(blink::WebNavigationPolicy);
  virtual void runModal() {}
  virtual blink::WebRect windowRect();
  virtual void setToolTipText(const blink::WebString& text,
                              blink::WebTextDirection hint);
  virtual void setWindowRect(const blink::WebRect&);
  virtual blink::WebRect windowResizerRect();
  virtual blink::WebRect rootWindowRect();
  virtual blink::WebScreenInfo screenInfo();
  virtual float deviceScaleFactor();
  virtual void resetInputMethod();
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool event_cancelled);

  // Called when a plugin is moved.  These events are queued up and sent with
  // the next paint or scroll message to the host.
  void SchedulePluginMove(const WebPluginGeometry& move);

  // Called when a plugin window has been destroyed, to make sure the currently
  // pending moves don't try to reference it.
  void CleanupWindowInPluginMoves(gfx::PluginWindowHandle window);

  RenderWidgetCompositor* compositor() const;

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback);

  // Callback for use with synthetic gestures (e.g. BeginSmoothScroll).
  typedef base::Callback<void()> SyntheticGestureCompletionCallback;

  // Send a synthetic gesture to the browser to be queued to the synthetic
  // gesture controller.
  void QueueSyntheticGesture(
      scoped_ptr<SyntheticGestureParams> gesture_params,
      const SyntheticGestureCompletionCallback& callback);

  // Close the underlying WebWidget.
  virtual void Close();

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame();

  // Handle common setup/teardown for handling IME events.
  void StartHandlingImeEvent();
  void FinishHandlingImeEvent();

  // Returns whether we currently should handle an IME event.
  bool ShouldHandleImeEvent();

  virtual void InstrumentWillBeginFrame(int frame_id) {}
  virtual void InstrumentDidBeginFrame() {}
  virtual void InstrumentDidCancelFrame() {}
  virtual void InstrumentWillComposite() {}

  bool UsingSynchronousRendererCompositor() const;

  // ScreenMetricsEmulator class manages screen emulation inside a render
  // widget. This includes resizing, placing view on the screen at desired
  // position, changing device scale factor, and scaling down the whole
  // widget if required to fit into the browser window.
  class ScreenMetricsEmulator;

  // Emulates screen and widget metrics. Supplied values override everything
  // coming from host.
  void EnableScreenMetricsEmulation(
      const blink::WebDeviceEmulationParams& params);
  void DisableScreenMetricsEmulation();
  void SetPopupOriginAdjustmentsForEmulation(ScreenMetricsEmulator* emulator);

  void ScheduleCompositeWithForcedRedraw();

  // Called by the compositor in single-threaded mode when a swap is posted,
  // completes or is aborted.
  void OnSwapBuffersPosted();
  void OnSwapBuffersComplete();
  void OnSwapBuffersAborted();

  // Checks if the text input state and compose inline mode have been changed.
  // If they are changed, the new value will be sent to the browser process.
  void UpdateTextInputType();

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  void OnShowHostContextMenu(ContextMenuParams* params);

#if defined(OS_ANDROID) || defined(USE_AURA)
  // |show_ime_if_needed| should be true iff the update may cause the ime to be
  // displayed, e.g. after a tap on an input field on mobile.
  // |send_ime_ack| should be true iff the browser side is required to
  // acknowledge the change before the renderer handles any more IME events.
  // This is when the event did not originate from the browser side IME, such as
  // changes from JavaScript or autofill.
  void UpdateTextInputState(bool show_ime_if_needed, bool send_ime_ack);
#endif

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process.
  void UpdateCompositionInfo(bool should_update_range);
#endif

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

  RenderWidget(blink::WebPopupType popup_type,
               const blink::WebScreenInfo& screen_info,
               bool swapped_out,
               bool hidden);

  virtual ~RenderWidget();

  // Initializes this view with the given opener.  CompleteInit must be called
  // later.
  bool Init(int32 opener_id);

  // Called by Init and subclasses to perform initialization.
  bool DoInit(int32 opener_id,
              blink::WebWidget* web_widget,
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
  void FlushPendingInputEventAck();
  void DoDeferredUpdateAndSendInputAck();
  void DoDeferredUpdate();
  void DoDeferredClose();
  void DoDeferredSetWindowRect(const blink::WebRect& pos);
  virtual void Composite(base::TimeTicks frame_begin_time);

  // Set the background of the render widget to a bitmap. The bitmap will be
  // tiled in both directions if it isn't big enough to fill the area. This is
  // mainly intended to be used in conjuction with WebView::SetIsTransparent().
  virtual void SetBackground(const SkBitmap& bitmap);

  // Resizes the render widget.
  void Resize(const gfx::Size& new_size,
              const gfx::Size& physical_backing_size,
              float overdraw_bottom_height,
              const gfx::Rect& resizer_rect,
              bool is_fullscreen,
              ResizeAck resize_ack);
  // Used to force the size of a window when running layout tests.
  void ResizeSynchronously(const gfx::Rect& new_position);
  virtual void SetScreenMetricsEmulationParameters(
      float device_scale_factor,
      const gfx::Point& root_layer_offset,
      float root_layer_scale);
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void SetExternalPopupOriginAdjustmentsForEmulation(
      ExternalPopupMenu* popup, ScreenMetricsEmulator* emulator);
#endif

  // RenderWidget IPC message handlers
  void OnHandleInputEvent(const blink::WebInputEvent* event,
                          const ui::LatencyInfo& latency_info,
                          bool keyboard_shortcut);
  void OnCursorVisibilityChange(bool is_visible);
  void OnMouseCaptureLost();
  virtual void OnSetFocus(bool enable);
  void OnClose();
  void OnCreatingNewAck();
  virtual void OnResize(const ViewMsg_Resize_Params& params);
  void OnChangeResizeRect(const gfx::Rect& resizer_rect);
  virtual void OnWasHidden();
  virtual void OnWasShown(bool needs_repainting);
  virtual void OnWasSwappedOut();
  void OnUpdateRectAck();
  void OnCreateVideoAck(int32 video_id);
  void OnUpdateVideoAck(int32 video_id);
  void OnRequestMoveAck();
  void OnSetInputMethodActive(bool is_active);
  void OnCandidateWindowShown();
  void OnCandidateWindowUpdated();
  void OnCandidateWindowHidden();
  virtual void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  virtual void OnImeConfirmComposition(const base::string16& text,
                                       const gfx::Range& replacement_range,
                                       bool keep_selection);
  void OnRepaint(gfx::Size size_to_paint);
  void OnSyntheticGestureCompleted();
  void OnSetTextDirection(blink::WebTextDirection direction);
  void OnGetFPS();
  void OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                           const gfx::Rect& window_screen_rect);
#if defined(OS_ANDROID)
  void OnShowImeIfNeeded();

  // Whenever an IME event that needs an acknowledgement is sent to the browser,
  // the number of outstanding IME events that needs acknowledgement should be
  // incremented. All IME events will be dropped until we receive an ack from
  // the browser.
  void IncrementOutstandingImeEventAcks();

  // Called by the browser process for every required IME acknowledgement.
  void OnImeEventAck();
#endif

  void OnSnapshot(const gfx::Rect& src_subrect);

  // Notify the compositor about a change in viewport size. This should be
  // used only with auto resize mode WebWidgets, as normal WebWidgets should
  // go through OnResize.
  void AutoResizeCompositor();

  virtual void SetDeviceScaleFactor(float device_scale_factor);

  // Override points to notify derived classes that a paint has happened.
  // DidInitiatePaint happens when that has completed, and subsequent rendering
  // won't affect the painted content. DidFlushPaint happens once we've received
  // the ACK that the screen has been updated. For a given paint operation,
  // these overrides will always be called in the order DidInitiatePaint,
  // DidFlushPaint.
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

  // Override point to obtain that the current input method state and caret
  // position.
  virtual ui::TextInputType GetTextInputType();
  virtual ui::TextInputType WebKitToUiTextInputType(
      blink::WebTextInputType type);

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  // Override point to obtain that the current composition character bounds.
  // In the case of surrogate pairs, the character is treated as two characters:
  // the bounds for first character is actual one, and the bounds for second
  // character is zero width rectangle.
  virtual void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds);

  // Returns the range of the text that is being composed or the selection if
  // the composition does not exist.
  virtual void GetCompositionRange(gfx::Range* range);

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds);
#endif

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
  void SetPendingWindowRect(const blink::WebRect& r);

  // Called by OnHandleInputEvent() to notify subclasses that a key event was
  // just handled.
  virtual void DidHandleKeyEvent() {}

  // Called by OnHandleInputEvent() to notify subclasses that a user gesture
  // event will be processed.
  virtual void WillProcessUserGesture() {}

  // Called by OnHandleInputEvent() to notify subclasses that a mouse event is
  // about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit or trigger DidHandleMouseEvent().
  virtual bool WillHandleMouseEvent(const blink::WebMouseEvent& event);

  // Called by OnHandleInputEvent() to notify subclasses that a gesture event is
  // about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit.
  virtual bool WillHandleGestureEvent(const blink::WebGestureEvent& event);

  // Called by OnHandleInputEvent() to notify subclasses that a mouse event was
  // just handled.
  virtual void DidHandleMouseEvent(const blink::WebMouseEvent& event) {}

  // Called by OnHandleInputEvent() to notify subclasses that a touch event was
  // just handled.
  virtual void DidHandleTouchEvent(const blink::WebTouchEvent& event) {}

  // Check whether the WebWidget has any touch event handlers registered
  // at the given point.
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const;

  // Check whether the WebWidget has any touch event handlers registered.
  virtual void hasTouchEventHandlers(bool has_handlers);

  // Tell the browser about the actions permitted for a new touch point.
  virtual void setTouchAction(blink::WebTouchAction touch_action);

#if defined(OS_ANDROID)
  // Checks if the selection root bounds have changed. If they have changed, the
  // new value will be sent to the browser process.
  virtual void UpdateSelectionRootBounds();
#endif

  // Creates a 3D context associated with this view.
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateGraphicsContext3D();

  bool OnSnapshotHelper(const gfx::Rect& src_subrect, SkBitmap* bitmap);

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost. When MSG_ROUTING_NONE, no messages may be sent.
  int32 routing_id_;

  int32 surface_id_;

  // We are responsible for destroying this object via its Close method.
  // May be NULL when the window is closing.
  blink::WebWidget* webwidget_;

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

  // The height of the physical backing surface that is overdrawn opaquely in
  // the browser, for example by an on-screen-keyboard (in DPI-adjusted pixels).
  float overdraw_bottom_height_;

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

  // Are we currently handling a touchstart event?
  bool handling_touchstart_event_;

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
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current selection root bounds.
#if defined(OS_ANDROID)
  gfx::Rect selection_root_rect_;
#endif

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_;

  // The kind of popup this widget represents, NONE if not a popup.
  blink::WebPopupType popup_type_;

  // Holds all the needed plugin window moves for a scroll.
  typedef std::vector<WebPluginGeometry> WebPluginGeometryVector;
  WebPluginGeometryVector plugin_window_moves_;

  // A custom background for the widget.
  SkBitmap background_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  blink::WebRect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect view_screen_rect_;
  gfx::Rect window_screen_rect_;

  scoped_ptr<IPC::Message> pending_input_event_ack_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_;

  // Set to true if painting to the window is handled by the accelerated
  // compositor.
  bool is_accelerated_compositing_active_;

  // Set to true if compositing has ever been active for this widget. Once a
  // widget has used compositing, it will act as though force compositing mode
  // is on for the remainder of the widget's lifetime.
  bool was_accelerated_compositing_ever_active_;

  base::OneShotTimer<RenderWidget> animation_timer_;
  base::Time animation_floor_time_;
  bool animation_update_pending_;
  bool invalidation_task_posted_;

  bool has_disable_gpu_vsync_switch_;
  base::TimeTicks last_do_deferred_update_time_;

  // Stats for legacy software mode
  scoped_ptr<cc::RenderingStatsInstrumentation> legacy_software_mode_stats_;

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
  blink::WebScreenInfo screen_info_;

  // The device scale factor. This value is computed from the DPI entries in
  // |screen_info_| on some platforms, and defaults to 1 on other platforms.
  float device_scale_factor_;

  // State associated with synthetic gestures. Synthetic gestures are processed
  // in-order, so a queue is sufficient to identify the correct state for a
  // completed gesture.
  std::queue<SyntheticGestureCompletionCallback>
      pending_synthetic_gesture_callbacks_;

  // Specified whether the compositor will run in its own thread.
  bool is_threaded_compositing_enabled_;

  // The latency information for any current non-accelerated-compositing
  // frame.
  std::vector<ui::LatencyInfo> latency_info_;

  uint32 next_output_surface_id_;

#if defined(OS_ANDROID)
  // A counter for number of outstanding messages from the renderer to the
  // browser regarding IME-type events that have not been acknowledged by the
  // browser. If this value is not 0 IME events will be dropped.
  int outstanding_ime_acks_;
#endif

  scoped_ptr<ScreenMetricsEmulator> screen_metrics_emulator_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These values are used to properly adjust popup position.
  gfx::Point popup_view_origin_for_emulation_;
  gfx::Point popup_screen_origin_for_emulation_;
  float popup_origin_scale_for_emulation_;

  scoped_ptr<ResizingModeSelector> resizing_mode_selector_;

  // A list of swapped out RenderFrames that need to be notified
  // of compositing-related events (e.g. DidCommitCompositorFrame).
  ObserverList<RenderFrameImpl> swapped_out_frames_;

  ui::MenuSourceType context_menu_source_type_;
  gfx::Point touch_editing_context_menu_location_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
