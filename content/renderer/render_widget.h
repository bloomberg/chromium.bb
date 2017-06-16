// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "cc/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/edit_command.h"
#include "content/common/features.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/screen_info.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator_delegate.h"
#include "content/renderer/gpu/render_widget_compositor_delegate.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/input/render_widget_input_handler.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/message_delivery_policy.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_mouse_lock_dispatcher.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebTextInputInfo.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

class GURL;

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
namespace scheduler {
class RenderWidgetSchedulingState;
}
struct WebActiveWheelFlingParameters;
struct WebDeviceEmulationParams;
class WebDragData;
class WebFrameWidget;
class WebGestureEvent;
class WebImage;
class WebInputMethodController;
class WebLocalFrame;
class WebMouseEvent;
class WebNode;
struct WebPoint;
}  // namespace blink

namespace cc {
class SwapPromise;
}

namespace gfx {
class Range;
}

namespace ui {
struct DidOverscrollParams;
}

namespace content {
class CompositorDependencies;
class ExternalPopupMenu;
class FrameSwapMessageQueue;
class ImeEventGuard;
class MainThreadEventQueue;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderViewImpl;
class RenderWidgetCompositor;
class RenderWidgetOwnerDelegate;
class RenderWidgetScreenMetricsEmulator;
class ResizingModeSelector;
class TextInputClientObserver;
struct ContextMenuParams;
struct ResizeParams;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
//
// RenderWidget is used to implement:
// - RenderViewImpl (deprecated)
// - Fullscreen mode (RenderWidgetFullScreen)
// - Popup "menus" (like the color chooser and date picker)
// - Widgets for frames (for out-of-process iframe support)
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      NON_EXPORTED_BASE(virtual public blink::WebWidgetClient),
      public RenderWidgetCompositorDelegate,
      public RenderWidgetInputHandlerDelegate,
      public RenderWidgetScreenMetricsEmulatorDelegate,
      public base::RefCounted<RenderWidget>,
      public MainThreadEventQueueClient {
 public:
  // Creates a new RenderWidget for a popup. |opener| is the RenderView that
  // this widget lives inside.
  static RenderWidget* CreateForPopup(RenderViewImpl* opener,
                                      CompositorDependencies* compositor_deps,
                                      blink::WebPopupType popup_type,
                                      const ScreenInfo& screen_info);

  // Creates a new RenderWidget that will be attached to a RenderFrame.
  static RenderWidget* CreateForFrame(int widget_routing_id,
                                      bool hidden,
                                      const ScreenInfo& screen_info,
                                      CompositorDependencies* compositor_deps,
                                      blink::WebLocalFrame* frame);

  // Used by content_layouttest_support to hook into the creation of
  // RenderWidgets.
  using CreateRenderWidgetFunction = RenderWidget* (*)(int32_t,
                                                       CompositorDependencies*,
                                                       blink::WebPopupType,
                                                       const ScreenInfo&,
                                                       bool,
                                                       bool,
                                                       bool);
  using RenderWidgetInitializedCallback = void (*)(RenderWidget*);
  using ShowCallback = base::Callback<void(RenderWidget* widget_to_show,
                                           blink::WebNavigationPolicy policy,
                                           const gfx::Rect& initial_rect)>;
  static void InstallCreateHook(
      CreateRenderWidgetFunction create_render_widget,
      RenderWidgetInitializedCallback render_widget_initialized_callback);

  // Closes a RenderWidget that was created by |CreateForFrame|.
  // TODO(avi): De-virtualize this once RenderViewImpl has-a RenderWidget.
  // https://crbug.com/545684
  virtual void CloseForFrame();

  int32_t routing_id() const { return routing_id_; }

  CompositorDependencies* compositor_deps() const { return compositor_deps_; }
  virtual blink::WebWidget* GetWebWidget() const;

  // Returns the current instance of WebInputMethodController which is to be
  // used for IME related tasks. This instance corresponds to the one from
  // focused frame and can be nullptr.
  blink::WebInputMethodController* GetInputMethodController() const;

  const gfx::Size& size() const { return size_; }
  bool is_fullscreen_granted() const { return is_fullscreen_granted_; }
  blink::WebDisplayMode display_mode() const { return display_mode_; }
  bool is_hidden() const { return is_hidden_; }
  // Temporary for debugging purposes...
  bool closing() const { return closing_; }
  bool has_host_context_menu_location() const {
    return has_host_context_menu_location_;
  }
  gfx::Point host_context_menu_location() const {
    return host_context_menu_location_;
  }

  void set_owner_delegate(RenderWidgetOwnerDelegate* owner_delegate) {
    DCHECK(!owner_delegate_);
    owner_delegate_ = owner_delegate;
  }

  RenderWidgetOwnerDelegate* owner_delegate() const { return owner_delegate_; }

  // ScreenInfo exposed so it can be passed to subframe RenderWidgets.
  ScreenInfo screen_info() const { return screen_info_; }

  // Sets whether this RenderWidget has been swapped out to be displayed by
  // a RenderWidget in a different process.  If so, no new IPC messages will be
  // sent (only ACKs) and the process is free to exit when there are no other
  // active RenderWidgets.
  void SetSwappedOut(bool is_swapped_out);

  bool is_swapped_out() const { return is_swapped_out_; }

  // Manage edit commands to be used for the next keyboard event.
  const EditCommands& edit_commands() const { return edit_commands_; }
  void SetEditCommandForNextKeyEvent(const std::string& name,
                                     const std::string& value);
  void ClearEditCommands();

  // Functions to track out-of-process frames for special notifications.
  void RegisterRenderFrameProxy(RenderFrameProxy* proxy);
  void UnregisterRenderFrameProxy(RenderFrameProxy* proxy);

  // Functions to track all RenderFrame objects associated with this
  // RenderWidget.
  void RegisterRenderFrame(RenderFrameImpl* frame);
  void UnregisterRenderFrame(RenderFrameImpl* frame);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // RenderWidgetCompositorDelegate
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
  void BeginMainFrame(double frame_time_sec) override;
  void RequestNewCompositorFrameSink(
      bool fallback,
      const CompositorFrameSinkCallback& callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  void DidCommitCompositorFrame() override;
  void DidCompletePageScaleAnimation() override;
  void DidReceiveCompositorFrameAck() override;
  bool IsClosing() const override;
  void RequestScheduleAnimation() override;
  void UpdateVisualState() override;
  void WillBeginCompositorFrame() override;
  std::unique_ptr<cc::SwapPromise> RequestCopyOfOutputForLayoutTest(
      std::unique_ptr<cc::CopyOutputRequest> request) override;

  // RenderWidgetInputHandlerDelegate
  void FocusChangeComplete() override;
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override;
  void ObserveGestureEventAndResult(const blink::WebGestureEvent& gesture_event,
                                    const gfx::Vector2dF& unused_delta,
                                    bool event_processed) override;

  void OnDidHandleKeyEvent() override;
  void OnDidOverscroll(const ui::DidOverscrollParams& params) override;
  void SetInputHandler(RenderWidgetInputHandler* input_handler) override;
  void ShowVirtualKeyboard() override;
  void UpdateTextInputState() override;
  void ClearTextInputState() override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;

  // RenderWidgetScreenMetricsDelegate
  void Redraw() override;
  void Resize(const ResizeParams& resize_params) override;
  void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override;
  void SetScreenRects(const gfx::Rect& view_screen_rect,
                      const gfx::Rect& window_screen_rect) override;

  // blink::WebWidgetClient
  blink::WebLayerTreeView* InitializeLayerTreeView() override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidChangeCursor(const blink::WebCursorInfo&) override;
  void AutoscrollStart(const blink::WebFloatPoint& point) override;
  void AutoscrollFling(const blink::WebFloatSize& velocity) override;
  void AutoscrollEnd() override;
  void CloseWidgetSoon() override;
  void Show(blink::WebNavigationPolicy) override;
  blink::WebRect WindowRect() override;
  blink::WebRect ViewRect() override;
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override;
  void SetWindowRect(const blink::WebRect&) override;
  blink::WebScreenInfo GetScreenInfo() override;
  void DidHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void DidOverscroll(const blink::WebFloatSize& overscrollDelta,
                     const blink::WebFloatSize& accumulatedOverscroll,
                     const blink::WebFloatPoint& position,
                     const blink::WebFloatSize& velocity) override;
  void ShowVirtualKeyboardOnElementFocus() override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  void ConvertWindowToViewport(blink::WebFloatRect* rect) override;
  bool RequestPointerLock() override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void StartDragging(blink::WebReferrerPolicy policy,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const blink::WebImage& image,
                     const blink::WebPoint& imageOffset) override;

  // Override point to obtain that the current input method state and caret
  // position.
  virtual ui::TextInputType GetTextInputType();

#if defined(OS_ANDROID)
  // Notifies that a tap was not consumed, so showing a UI for the unhandled
  // tap may be needed.
  // Performs various checks on the given WebNode to apply heuristics to
  // determine if triggering is appropriate.
  void ShowUnhandledTapUIIfNeeded(const blink::WebPoint& tapped_position,
                                  const blink::WebNode& tapped_node,
                                  bool page_changed) override;
#endif

  // Begins the compositor's scheduler to start producing frames.
  void StartCompositor();

  // Stop compositing.
  void WillCloseLayerTreeView();

  RenderWidgetCompositor* compositor() const;

  const RenderWidgetInputHandler& input_handler() const {
    return *input_handler_;
  }

  void SetHandlingInputEventForTesting(bool handling_input_event);

  // Callback for use with synthetic gestures (e.g. BeginSmoothScroll).
  typedef base::Callback<void()> SyntheticGestureCompletionCallback;

  // Send a synthetic gesture to the browser to be queued to the synthetic
  // gesture controller.
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGestureParams> gesture_params,
      const SyntheticGestureCompletionCallback& callback);

  // Deliveres |message| together with compositor state change updates. The
  // exact behavior depends on |policy|.
  // This mechanism is not a drop-in replacement for IPC: messages sent this way
  // will not be automatically available to BrowserMessageFilter, for example.
  // FIFO ordering is preserved between messages enqueued with the same
  // |policy|, the ordering between messages enqueued for different policies is
  // undefined.
  //
  // |msg| message to send, ownership of |msg| is transferred.
  // |policy| see the comment on MessageDeliveryPolicy.
  void QueueMessage(IPC::Message* msg, MessageDeliveryPolicy policy);

  // Handle start and finish of IME event guard.
  void OnImeEventGuardStart(ImeEventGuard* guard);
  void OnImeEventGuardFinish(ImeEventGuard* guard);

  void SetPopupOriginAdjustmentsForEmulation(
      RenderWidgetScreenMetricsEmulator* emulator);

  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  void OnShowHostContextMenu(ContextMenuParams* params);

  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process. This method does nothing when the browser process is not able to
  // handle composition range and composition character bounds.
  // If immediate_request is true, render sends the latest composition info to
  // the browser even if the composition info is not changed.
  void UpdateCompositionInfo(bool immediate_request);

  // Called when the Widget has changed size as a result of an auto-resize.
  void DidAutoResize(const gfx::Size& new_size);

  // Indicates whether this widget has focus.
  bool has_focus() const { return has_focus_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

  // TODO(ekaramad): The reference to the focused pepper plugin will be removed
  // from RenderWidget. The purpose of having the reference here was to make IME
  // work for OOPIF (https://crbug.com/643727).
  void set_focused_pepper_plugin(PepperPluginInstanceImpl* plugin) {
    focused_pepper_plugin_ = plugin;
  }

  // When emulated, this returns original device scale factor.
  float GetOriginalDeviceScaleFactor() const;

  // Helper to convert |point| using ConvertWindowToViewport().
  gfx::Point ConvertWindowPointToViewport(const gfx::Point& point);

  virtual void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) {}

  uint32_t GetContentSourceId();
  void IncrementContentSourceId();

  // MainThreadEventQueueClient overrides.

  // Requests a BeginMainFrame callback from the compositor.
  void SetNeedsMainFrame() override;

  void HandleInputEvent(const blink::WebCoalescedInputEvent& input_event,
                        const ui::LatencyInfo& latency_info,
                        HandledEventCallback callback) override;

  scoped_refptr<MainThreadEventQueue> GetInputEventQueue();

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

  RenderWidget(int32_t widget_routing_id,
               CompositorDependencies* compositor_deps,
               blink::WebPopupType popup_type,
               const ScreenInfo& screen_info,
               bool swapped_out,
               bool hidden,
               bool never_visible);

  ~RenderWidget() override;

  static blink::WebFrameWidget* CreateWebFrameWidget(
      RenderWidget* render_widget,
      blink::WebLocalFrame* frame);

  // Creates a WebWidget based on the popup type.
  static blink::WebWidget* CreateWebWidget(RenderWidget* render_widget);

  // Called by Create() functions and subclasses to finish initialization.
  // |show_callback| will be invoked once WebWidgetClient::show() occurs, and
  // should be null if show() won't be triggered for this widget.
  void Init(const ShowCallback& show_callback, blink::WebWidget* web_widget);

  // Allows the process to exit once the unload handler has finished, if there
  // are no other active RenderWidgets.
  void WasSwappedOut();

  void DoDeferredClose();
  void NotifyOnClose();

  gfx::Size GetSizeForWebWidget() const;
  virtual void ResizeWebWidget();

  // Close the underlying WebWidget.
  virtual void Close();

  // Used to force the size of a window when running layout tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void SetExternalPopupOriginAdjustmentsForEmulation(
      ExternalPopupMenu* popup,
      RenderWidgetScreenMetricsEmulator* emulator);
#endif

  // RenderWidget IPC message handlers
  void OnHandleInputEvent(
      const blink::WebInputEvent* event,
      const std::vector<const blink::WebInputEvent*>& coalesced_events,
      const ui::LatencyInfo& latency_info,
      InputEventDispatchType dispatch_type);
  void OnCursorVisibilityChange(bool is_visible);
  void OnMouseCaptureLost();
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  virtual void OnSetFocus(bool enable);
  void OnClose();
  void OnCreatingNewAck();
  virtual void OnResize(const ResizeParams& params);
  void OnEnableDeviceEmulation(const blink::WebDeviceEmulationParams& params);
  void OnDisableDeviceEmulation();
  virtual void OnWasHidden();
  virtual void OnWasShown(bool needs_repainting,
                          const ui::LatencyInfo& latency_info);
  void OnCreateVideoAck(int32_t video_id);
  void OnUpdateVideoAck(int32_t video_id);
  void OnRequestMoveAck();
  // Request from browser to show context menu.
  virtual void OnShowContextMenu(ui::MenuSourceType source_type,
                                 const gfx::Point& location);
  virtual void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end);
  virtual void OnImeCommitText(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      const gfx::Range& replacement_range,
      int relative_cursor_pos);
  virtual void OnImeFinishComposingText(bool keep_selection);

  // Called when the device scale factor is changed, or the layer tree is
  // initialized.
  virtual void OnDeviceScaleFactorChanged();

  void OnRepaint(gfx::Size size_to_paint);
  void OnSyntheticGestureCompleted();
  void OnSetTextDirection(blink::WebTextDirection direction);
  void OnGetFPS();
  void OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                           const gfx::Rect& window_screen_rect);
  void OnUpdateWindowScreenRect(const gfx::Rect& window_screen_rect);
  void OnSetViewportIntersection(const gfx::Rect& viewport_intersection);
  // Real data that is dragged is not included at DragEnter time.
  void OnDragTargetDragEnter(
      const std::vector<DropData::Metadata>& drop_meta_data,
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers);
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            blink::WebDragOperationsMask operations_allowed,
                            int key_modifiers);
  void OnDragTargetDragLeave(const gfx::Point& client_point,
                             const gfx::Point& screen_point);
  void OnDragTargetDrop(const DropData& drop_data,
                        const gfx::Point& client_pt,
                        const gfx::Point& screen_pt,
                        int key_modifiers);
  void OnDragSourceEnded(const gfx::Point& client_point,
                         const gfx::Point& screen_point,
                         blink::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();

#if defined(OS_ANDROID)
  // Called by the browser process to update text input state.
  void OnRequestTextInputStateUpdate();
#endif

  // Called by the browser process to update the cursor and composition
  // information by sending InputHostMsg_ImeCompositionRangeChanged. If
  // |immediate_request| is true, an IPC is sent back with current state.
  // When |monitor_update| is true, then RenderWidget will send the updates
  // in each compositor frame when there are changes. Outside of compositor
  // frame updates, a change in text selection might also lead to an update for
  // composition info (when in monitor mode).
  void OnRequestCompositionUpdates(bool immediate_request,
                                   bool monitor_updates);

  // Notify the compositor about a change in viewport size. This should be
  // used only with auto resize mode WebWidgets, as normal WebWidgets should
  // go through OnResize.
  void AutoResizeCompositor();

  virtual void OnSetDeviceScaleFactor(float device_scale_factor);

  void OnOrientationChange();

  // Override points to notify derived classes that a paint has happened.
  // DidInitiatePaint happens when that has completed, and subsequent rendering
  // won't affect the painted content.
  virtual void DidInitiatePaint() {}

  virtual GURL GetURLForGraphicsContext3D();

  // Sets the "hidden" state of this widget.  All accesses to is_hidden_ should
  // use this method so that we can properly inform the RenderThread of our
  // state.
  void SetHidden(bool hidden);

  void DidToggleFullscreen();

  bool next_paint_is_resize_ack() const;
  void set_next_paint_is_resize_ack();
  void set_next_paint_is_repaint_ack();

  // QueueMessage implementation extracted into a static method for easy
  // testing.
  static std::unique_ptr<cc::SwapPromise> QueueMessageImpl(
      IPC::Message* msg,
      MessageDeliveryPolicy policy,
      FrameSwapMessageQueue* frame_swap_message_queue,
      scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
      int source_frame_number);

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

  // Override point to obtain that the current input method state about
  // composition text.
  virtual bool CanComposeInline();

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const blink::WebRect& r);

  // Check whether the WebWidget has any touch event handlers registered.
  void HasTouchEventHandlers(bool has_handlers) override;

  // Called to update whether low latency input mode is enabled or not.
  void SetNeedsLowLatencyInput(bool) override;

  // Tell the browser about the actions permitted for a new touch point.
  void SetTouchAction(cc::TouchAction touch_action) override;

  // Sends an ACK to the browser process during the next compositor frame.
  void OnWaitNextFrameForTests(int routing_id);

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost.
  const int32_t routing_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // Use GetWebWidget() instead of using webwidget_internal_ directly.
  // We are responsible for destroying this object via its Close method.
  // May be NULL when the window is closing.
  blink::WebWidget* webwidget_internal_;

  // The delegate of the owner of this object.
  RenderWidgetOwnerDelegate* owner_delegate_;

  // This is lazily constructed and must not outlive webwidget_.
  std::unique_ptr<RenderWidgetCompositor> compositor_;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;

  // The size of the RenderWidget.
  gfx::Size size_;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  gfx::Size physical_backing_size_;

  // The size of the visible viewport in DPI-adjusted pixels.
  gfx::Size visible_viewport_size_;

  // Flags for the next ViewHostMsg_UpdateRect message.
  int next_paint_flags_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_;

  // True if we need to send an UpdateRect message to notify the browser about
  // an already-completed auto-resize.
  bool need_update_rect_for_auto_resize_;

  // Set to true if we should ignore RenderWidget::Show calls.
  bool did_show_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool compositor_never_visible_;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_;

  // Indicates the display mode.
  blink::WebDisplayMode display_mode_;

  // It is possible that one ImeEventGuard is nested inside another
  // ImeEventGuard. We keep track of the outermost one, and update it as needed.
  ImeEventGuard* ime_event_guard_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // True if it is known that the host is in the process of being shut down.
  bool host_closing_;

  // Whether this RenderWidget is currently swapped out, such that the view is
  // being rendered by another process.  If all RenderWidgets in a process are
  // swapped out, the process can exit.
  bool is_swapped_out_;

  // Whether this RenderWidget is for an out-of-process iframe or not.
  bool for_oopif_;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_;

  // The kind of popup this widget represents, NONE if not a popup.
  blink::WebPopupType popup_type_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  gfx::Rect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect view_screen_rect_;
  gfx::Rect window_screen_rect_;

  std::unique_ptr<RenderWidgetInputHandler> input_handler_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // Properties of the screen hosting this RenderWidget instance.
  ScreenInfo screen_info_;

  // The device scale factor. This value is computed from the DPI entries in
  // |screen_info_| on some platforms, and defaults to 1 on other platforms.
  float device_scale_factor_;

  // State associated with synthetic gestures. Synthetic gestures are processed
  // in-order, so a queue is sufficient to identify the correct state for a
  // completed gesture.
  std::queue<SyntheticGestureCompletionCallback>
      pending_synthetic_gesture_callbacks_;

  // True if the IME requests updated composition info.
  bool monitor_composition_info_;

  std::unique_ptr<RenderWidgetScreenMetricsEmulator> screen_metrics_emulator_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These values are used to properly adjust popup position.
  gfx::Point popup_view_origin_for_emulation_;
  gfx::Point popup_screen_origin_for_emulation_;
  float popup_origin_scale_for_emulation_;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;
  std::unique_ptr<ResizingModeSelector> resizing_mode_selector_;

  // Lists of RenderFrameProxy objects that need to be notified of
  // compositing-related events (e.g. DidCommitCompositorFrame).
  base::ObserverList<RenderFrameProxy> render_frame_proxies_;

  // A list of RenderFrames associated with this RenderWidget. Notifications
  // are sent to each frame in the list for events such as changing
  // visibility state for example.
  base::ObserverList<RenderFrameImpl> render_frames_;

  bool has_host_context_menu_location_;
  gfx::Point host_context_menu_location_;

  std::unique_ptr<blink::scheduler::RenderWidgetSchedulingState>
      render_widget_scheduling_state_;

  // Mouse Lock dispatcher attached to this view.
  std::unique_ptr<RenderWidgetMouseLockDispatcher> mouse_lock_dispatcher_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  std::unique_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  bool has_added_input_handler_;

 private:
  // Applies/Removes the DevTools device emulation transformation to/from a
  // window rect.
  void ScreenRectToEmulatedIfNeeded(blink::WebRect* window_rect) const;
  void EmulatedToScreenRectIfNeeded(blink::WebRect* window_rect) const;

  bool CreateWidget(int32_t opener_id,
                    blink::WebPopupType popup_type,
                    int32_t* routing_id);

  // A variant of Send but is fatal if it fails. The browser may
  // be waiting for this IPC Message and if the send fails the browser will
  // be left in a state waiting for something that never comes. And if it
  // never comes then it may later determine this is a hung renderer; so
  // instead fail right away.
  void SendOrCrash(IPC::Message* msg);

  // Determines whether or not RenderWidget should process IME events from the
  // browser. It always returns true unless there is no WebFrameWidget to
  // handle the event, or there is no page focus.
  bool ShouldHandleImeEvents() const;

  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool reply_to_request);

  gfx::ColorSpace GetRasterColorSpace() const;

  void SendInputEventAck(blink::WebInputEvent::Type type,
                         uint32_t touch_event_id,
                         InputEventAckState ack_state,
                         const ui::LatencyInfo& latency_info,
                         std::unique_ptr<ui::DidOverscrollParams>);

  // Indicates whether this widget has focus.
  bool has_focus_;

  // A callback into the creator/opener of this widget, to be executed when
  // WebWidgetClient::show() occurs.
  ShowCallback show_callback_;

#if defined(OS_MACOSX)
  // Responds to IPCs from TextInputClientMac regarding getting string at given
  // position or range as well as finding character index at a given position.
  std::unique_ptr<TextInputClientObserver> text_input_client_observer_;
#endif

  // This reference is set by the RenderFrame and is used to query the IME-
  // related state from the plugin to later send to the browser.
  PepperPluginInstanceImpl* focused_pepper_plugin_;

  // Stores edit commands associated to the next key event.
  // Will be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  bool time_to_first_active_paint_recorded_;
  base::TimeTicks was_shown_time_;

  // This is initialized to zero and is incremented on each non-same-page
  // navigation commit by RenderFrameImpl. At that time it is sent to the
  // compositor so that it can tag compositor frames, and RenderFrameImpl is
  // responsible for sending it to the browser process to be used to match
  // each compositor frame to the most recent page navigation before it was
  // generated.
  // This only applies to main frames, and is not touched for subframe
  // RenderWidgets, where there is no concern around displaying unloaded
  // content.
  // TODO(kenrb, fsamuel): This should be removed when SurfaceIDs can be used
  // to replace it. See https://crbug.com/695579.
  uint32_t current_content_source_id_;

  cc::LocalSurfaceId local_surface_id_;

  scoped_refptr<MainThreadEventQueue> input_event_queue_;

  base::WeakPtrFactory<RenderWidget> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
