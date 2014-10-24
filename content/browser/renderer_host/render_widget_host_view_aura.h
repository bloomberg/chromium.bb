// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/compositor/delegated_frame_host.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_delegate.h"

namespace aura {
class WindowTracker;
namespace client {
class ScopedTooltipDisabler;
}
}

namespace cc {
class CopyOutputRequest;
class CopyOutputResult;
class DelegatedFrameData;
}

namespace gfx {
class Canvas;
class Display;
}

namespace gpu {
struct Mailbox;
}

namespace ui {
class CompositorLock;
class InputMethod;
class LocatedEvent;
class Texture;
}

namespace content {
#if defined(OS_WIN)
class LegacyRenderWidgetHostHWND;
#endif

class OverscrollController;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostView;

// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewAura
    : public RenderWidgetHostViewBase,
      public DelegatedFrameHostClient,
      public ui::TextInputClient,
      public gfx::DisplayObserver,
      public aura::WindowTreeHostObserver,
      public aura::WindowDelegate,
      public aura::client::ActivationDelegate,
      public aura::client::ActivationChangeObserver,
      public aura::client::FocusChangeObserver,
      public aura::client::CursorClientObserver,
      public base::SupportsWeakPtr<RenderWidgetHostViewAura> {
 public:
  // Displays and controls touch editing elements such as selection handles.
  class TouchEditingClient {
   public:
    TouchEditingClient() {}

    // Tells the client to start showing touch editing handles.
    virtual void StartTouchEditing() = 0;

    // Notifies the client that touch editing is no longer needed. |quick|
    // determines whether the handles should fade out quickly or slowly.
    virtual void EndTouchEditing(bool quick) = 0;

    // Notifies the client that the selection bounds need to be updated.
    virtual void OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                            const gfx::Rect& focus) = 0;

    // Notifies the client that the current text input type as changed.
    virtual void OnTextInputTypeChanged(ui::TextInputType type) = 0;

    // Notifies the client that an input event is about to be sent to the
    // renderer. Returns true if the client wants to stop event propagation.
    virtual bool HandleInputEvent(const ui::Event* event) = 0;

    // Notifies the client that a gesture event ack was received.
    virtual void GestureEventAck(int gesture_event_type) = 0;

    // Notifies the client that the fling has ended, so it can activate touch
    // editing if needed.
    virtual void DidStopFlinging() = 0;

    // This is called when the view is destroyed, so that the client can
    // perform any necessary clean-up.
    virtual void OnViewDestroyed() = 0;

   protected:
    virtual ~TouchEditingClient() {}
  };

  void set_touch_editing_client(TouchEditingClient* client) {
    touch_editing_client_ = client;
  }

  // When |is_guest_view_hack| is true, this view isn't really the view for
  // the |widget|, a RenderWidgetHostViewGuest is.
  //
  // TODO(lazyboy): Remove |is_guest_view_hack| once BrowserPlugin has migrated
  // to use RWHVChildFrame (http://crbug.com/330264).
  RenderWidgetHostViewAura(RenderWidgetHost* host, bool is_guest_view_hack);

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) override;
  virtual void InitAsChild(gfx::NativeView parent_view) override;
  virtual RenderWidgetHost* GetRenderWidgetHost() const override;
  virtual void SetSize(const gfx::Size& size) override;
  virtual void SetBounds(const gfx::Rect& rect) override;
  virtual gfx::Vector2dF GetLastScrollOffset() const override;
  virtual gfx::NativeView GetNativeView() const override;
  virtual gfx::NativeViewId GetNativeViewId() const override;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() override;
  virtual ui::TextInputClient* GetTextInputClient() override;
  virtual bool HasFocus() const override;
  virtual bool IsSurfaceAvailableForCopy() const override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual bool IsShowing() override;
  virtual gfx::Rect GetViewBounds() const override;
  virtual void SetBackgroundColor(SkColor color) override;
  virtual gfx::Size GetVisibleViewportSize() const override;
  virtual void SetInsets(const gfx::Insets& insets) override;

  // Overridden from RenderWidgetHostViewBase:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) override;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) override;
  virtual void WasShown() override;
  virtual void WasHidden() override;
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) override;
  virtual void Focus() override;
  virtual void Blur() override;
  virtual void UpdateCursor(const WebCursor& cursor) override;
  virtual void SetIsLoading(bool is_loading) override;
  virtual void TextInputTypeChanged(ui::TextInputType type,
                                    ui::TextInputMode input_mode,
                                    bool can_compose_inline,
                                    int flags) override;
  virtual void ImeCancelComposition() override;
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) override;
  virtual void Destroy() override;
  virtual void SetTooltipText(const base::string16& tooltip_text) override;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) override;
  virtual gfx::Size GetRequestedRendererSize() const override;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      CopyFromCompositingSurfaceCallback& callback,
      const SkColorType color_type) override;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) override;
  virtual bool CanCopyToVideoFrame() const override;
  virtual bool CanSubscribeFrame() const override;
  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) override;
  virtual void EndFrameSubscription() override;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) override;
  virtual gfx::Rect GetBoundsInRootWindow() override;
  virtual void WheelEventAck(const blink::WebMouseWheelEvent& event,
                             InputEventAckState ack_result) override;
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result) override;
  virtual void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      InputEventAckState ack_result) override;
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() override;
  virtual BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate) override;
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible()
      override;
  virtual void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                                       const SkBitmap& zoomed_bitmap) override;
  virtual bool LockMouse() override;
  virtual void UnlockMouse() override;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) override;
  virtual void DidStopFlinging() override;

#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) override;
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const override;
#endif

  // Overridden from ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) override;
  virtual void ConfirmCompositionText() override;
  virtual void ClearCompositionText() override;
  virtual void InsertText(const base::string16& text) override;
  virtual void InsertChar(base::char16 ch, int flags) override;
  virtual gfx::NativeWindow GetAttachedWindow() const override;
  virtual ui::TextInputType GetTextInputType() const override;
  virtual ui::TextInputMode GetTextInputMode() const override;
  virtual int GetTextInputFlags() const override;
  virtual bool CanComposeInline() const override;
  virtual gfx::Rect GetCaretBounds() const override;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const override;
  virtual bool HasCompositionText() const override;
  virtual bool GetTextRange(gfx::Range* range) const override;
  virtual bool GetCompositionTextRange(gfx::Range* range) const override;
  virtual bool GetSelectionRange(gfx::Range* range) const override;
  virtual bool SetSelectionRange(const gfx::Range& range) override;
  virtual bool DeleteRange(const gfx::Range& range) override;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const override;
  virtual void OnInputMethodChanged() override;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) override;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) override;
  virtual void OnCandidateWindowShown() override;
  virtual void OnCandidateWindowUpdated() override;
  virtual void OnCandidateWindowHidden() override;
  virtual bool IsEditingCommandEnabled(int command_id) override;
  virtual void ExecuteEditingCommand(int command_id) override;

  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayAdded(const gfx::Display& new_display) override;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) override;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) override;

  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const override;
  virtual gfx::Size GetMaximumSize() const override;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) override;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  virtual int GetNonClientComponent(const gfx::Point& point) const override;
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  virtual bool CanFocus() override;
  virtual void OnCaptureLost() override;
  virtual void OnPaint(gfx::Canvas* canvas) override;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  virtual void OnWindowDestroying(aura::Window* window) override;
  virtual void OnWindowDestroyed(aura::Window* window) override;
  virtual void OnWindowTargetVisibilityChanged(bool visible) override;
  virtual bool HasHitTestMask() const override;
  virtual void GetHitTestMask(gfx::Path* mask) const override;

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) override;
  virtual void OnMouseEvent(ui::MouseEvent* event) override;
  virtual void OnScrollEvent(ui::ScrollEvent* event) override;
  virtual void OnTouchEvent(ui::TouchEvent* event) override;
  virtual void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::client::ActivationDelegate:
  virtual bool ShouldActivate() const override;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_activation,
                                 aura::Window* lost_activation) override;

  // Overridden from aura::client::CursorClientObserver:
  virtual void OnCursorVisibilityChanged(bool is_visible) override;

  // Overridden from aura::client::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) override;

  // Overridden from aura::WindowTreeHostObserver:
  virtual void OnHostMoved(const aura::WindowTreeHost* host,
                           const gfx::Point& new_origin) override;

  void OnTextInputStateChanged(const ViewHostMsg_TextInputState_Params& params);

#if defined(OS_WIN)
  // Sets the cutout rects from constrained windows. These are rectangles that
  // windowed NPAPI plugins shouldn't paint in. Overwrites any previous cutout
  // rects.
  void UpdateConstrainedWindowRects(const std::vector<gfx::Rect>& rects);

  // Updates the cursor clip region. Used for mouse locking.
  void UpdateMouseLockRegion();

  // Notification that the LegacyRenderWidgetHostHWND was destroyed.
  void OnLegacyWindowDestroyed();
#endif

  void DisambiguationPopupRendered(bool success, const SkBitmap& result);

  void HideDisambiguationPopup();

  void ProcessDisambiguationGesture(ui::GestureEvent* event);

  void ProcessDisambiguationMouse(ui::MouseEvent* event);

  // Method to indicate if this instance is shutting down or closing.
  // TODO(shrikant): Discuss around to see if it makes sense to add this method
  // as part of RenderWidgetHostView.
  bool IsClosing() const { return in_shutdown_; }

  // Sets whether the overscroll controller should be enabled for this page.
  void SetOverscrollControllerEnabled(bool enabled);

  void SnapToPhysicalPixelBoundary();

  OverscrollController* overscroll_controller() const {
    return overscroll_controller_.get();
  }

 protected:
  virtual ~RenderWidgetHostViewAura();

  // Exposed for tests.
  aura::Window* window() { return window_; }
  virtual SkColorType PreferredReadbackFormat() override;
  virtual DelegatedFrameHost* GetDelegatedFrameHost() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           PopupRetainsCaptureAfterMouseRelease);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SetCompositionText);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventState);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           TouchEventPositionsArentRounded);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventSyncAsync);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SwapNotifiesWindow);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, OutputSurfaceIdChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SoftwareDPIChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           UpdateCursorIfOverSelf);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraCopyRequestTest,
                           DestroyedAfterCopyRequest);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           VisibleViewportTest);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           OverscrollResetsOnBlur);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           FinishCompositionByMouse);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest,
                           WebContentsViewReparent);

  class WindowObserver;
  friend class WindowObserver;

  void UpdateCursorIfOverSelf();

  // Tracks whether SnapToPhysicalPixelBoundary() has been called.
  bool has_snapped_to_boundary() { return has_snapped_to_boundary_; }
  void ResetHasSnappedToBoundary() { has_snapped_to_boundary_ = false; }

  // Set the bounds of the window and handle size changes.  Assumes the caller
  // has already adjusted the origin of |rect| to conform to whatever coordinate
  // space is required by the aura::Window.
  void InternalSetBounds(const gfx::Rect& rect);

#if defined(OS_WIN)
  bool UsesNativeWindowFrame() const;
#endif

  ui::InputMethod* GetInputMethod() const;

  // Returns whether the widget needs an input grab to work properly.
  bool NeedsInputGrab();

  // Returns whether the widget needs to grab mouse capture to work properly.
  bool NeedsMouseCapture();

  // Confirm existing composition text in the webpage and ask the input method
  // to cancel its ongoing composition session.
  void FinishImeCompositionSession();

  // This method computes movementX/Y and keeps track of mouse location for
  // mouse lock on all mouse move events.
  void ModifyEventMovementAndCoords(blink::WebMouseEvent* event);

  // Sends an IPC to the renderer process to communicate whether or not
  // the mouse cursor is visible anywhere on the screen.
  void NotifyRendererOfCursorVisibilityState(bool is_visible);

  // If |clip| is non-empty and and doesn't contain |rect| or |clip| is empty
  // SchedulePaint() is invoked for |rect|.
  void SchedulePaintIfNotInClip(const gfx::Rect& rect, const gfx::Rect& clip);

  // Helper method to determine if, in mouse locked mode, the cursor should be
  // moved to center.
  bool ShouldMoveToCenter();

  // Called after |window_| is parented to a WindowEventDispatcher.
  void AddedToRootWindow();

  // Called prior to removing |window_| from a WindowEventDispatcher.
  void RemovingFromRootWindow();

  // DelegatedFrameHostClient implementation.
  virtual ui::Compositor* GetCompositor() const override;
  virtual ui::Layer* GetLayer() override;
  virtual RenderWidgetHostImpl* GetHost() override;
  virtual bool IsVisible() override;
  virtual scoped_ptr<ResizeLock> CreateResizeLock(
      bool defer_compositor_lock) override;
  virtual gfx::Size DesiredFrameSize() override;
  virtual float CurrentDeviceScaleFactor() override;
  virtual gfx::Size ConvertViewSizeToPixel(const gfx::Size& size) override;

  // Detaches |this| from the input method object.
  void DetachFromInputMethod();

  // Before calling RenderWidgetHost::ForwardKeyboardEvent(), this method
  // calls our keybindings handler against the event and send matched
  // edit commands to renderer instead.
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Dismisses a Web Popup on a mouse or touch press outside the popup and its
  // parent.
  void ApplyEventFilterForPopupExit(ui::LocatedEvent* event);

  // Converts |rect| from window coordinate to screen coordinate.
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const;

  // Converts |rect| from screen coordinate to window coordinate.
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const;

  // Helper function to set keyboard focus to the main window.
  void SetKeyboardFocus();

  RenderFrameHostImpl* GetFocusedFrame();

  // The model object.
  RenderWidgetHostImpl* host_;

  aura::Window* window_;

  scoped_ptr<DelegatedFrameHost> delegated_frame_host_;

  scoped_ptr<WindowObserver> window_observer_;

  // Are we in the process of closing?  Tracked so fullscreen views can avoid
  // sending a second shutdown request to the host when they lose the focus
  // after requesting shutdown for another reason (e.g. Escape key).
  bool in_shutdown_;

  // True if in the process of handling a window bounds changed notification.
  bool in_bounds_changed_;

  // Is this a fullscreen view?
  bool is_fullscreen_;

  // Our parent host view, if this is a popup.  NULL otherwise.
  RenderWidgetHostViewAura* popup_parent_host_view_;

  // Our child popup host. NULL if we do not have a child popup.
  RenderWidgetHostViewAura* popup_child_host_view_;

  class EventFilterForPopupExit;
  friend class EventFilterForPopupExit;
  scoped_ptr<ui::EventHandler> event_filter_for_popup_exit_;

  // True when content is being loaded. Used to show an hourglass cursor.
  bool is_loading_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // The touch-event. Its touch-points are updated as necessary. A new
  // touch-point is added from an ET_TOUCH_PRESSED event, and a touch-point is
  // removed from the list on an ET_TOUCH_RELEASED event.
  blink::WebTouchEvent touch_event_;

  // The current text input type.
  ui::TextInputType text_input_type_;
  // The current text input mode corresponding to HTML5 inputmode attribute.
  ui::TextInputMode text_input_mode_;
  // The current text input flags.
  int text_input_flags_;
  bool can_compose_inline_;

  // Rectangles for the selection anchor and focus.
  gfx::Rect selection_anchor_rect_;
  gfx::Rect selection_focus_rect_;

  // The current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Indicates if there is onging composition text.
  bool has_composition_text_;

  // Whether return characters should be passed on to the RenderWidgetHostImpl.
  bool accept_return_character_;

  // Current tooltip text.
  base::string16 tooltip_;

  // The size and scale of the last software compositing frame that was swapped.
  gfx::Size last_swapped_software_frame_size_;
  float last_swapped_software_frame_scale_factor_;

  // If non-NULL we're in OnPaint() and this is the supplied canvas.
  gfx::Canvas* paint_canvas_;

  // Used to record the last position of the mouse.
  // While the mouse is locked, they store the last known position just as mouse
  // lock was entered.
  // Relative to the upper-left corner of the view.
  gfx::Point unlocked_mouse_position_;
  // Relative to the upper-left corner of the screen.
  gfx::Point unlocked_global_mouse_position_;
  // Last cursor position relative to screen. Used to compute movementX/Y.
  gfx::Point global_mouse_position_;
  // In mouse locked mode, we synthetically move the mouse cursor to the center
  // of the window when it reaches the window borders to avoid it going outside.
  // This flag is used to differentiate between these synthetic mouse move
  // events vs. normal mouse move events.
  bool synthetic_move_sent_;

  // Used to track the state of the window we're created from. Only used when
  // created fullscreen.
  scoped_ptr<aura::WindowTracker> host_tracker_;

  // Used to track the last cursor visibility update that was sent to the
  // renderer via NotifyRendererOfCursorVisibilityState().
  enum CursorVisibilityState {
    UNKNOWN,
    VISIBLE,
    NOT_VISIBLE,
  };
  CursorVisibilityState cursor_visibility_state_in_renderer_;

#if defined(OS_WIN)
  // The list of rectangles from constrained windows over this view. Windowed
  // NPAPI plugins shouldn't draw over them.
  std::vector<gfx::Rect> constrained_rects_;

  typedef std::map<HWND, WebPluginGeometry> PluginWindowMoves;
  // Contains information about each windowed plugin's clip and cutout rects (
  // from the renderer). This is needed because when the transient windows
  // over this view changes, we need this information in order to create a new
  // region for the HWND.
  PluginWindowMoves plugin_window_moves_;

  // The LegacyRenderWidgetHostHWND class provides a dummy HWND which is used
  // for accessibility, as the container for windowless plugins like
  // Flash/Silverlight, etc and for legacy drivers for trackpoints/trackpads,
  // etc.
  // The LegacyRenderWidgetHostHWND instance is created during the first call
  // to RenderWidgetHostViewAura::InternalSetBounds. The instance is destroyed
  // when the LegacyRenderWidgetHostHWND hwnd is destroyed.
  content::LegacyRenderWidgetHostHWND* legacy_render_widget_host_HWND_;

  // Set to true if the legacy_render_widget_host_HWND_ instance was destroyed
  // by Windows. This could happen if the browser window was destroyed by
  // DestroyWindow for e.g. This flag helps ensure that we don't try to create
  // the LegacyRenderWidgetHostHWND instance again as that would be a futile
  // exercise.
  bool legacy_window_destroyed_;
#endif

  bool has_snapped_to_boundary_;

  TouchEditingClient* touch_editing_client_;

  scoped_ptr<OverscrollController> overscroll_controller_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  gfx::Insets insets_;

  std::vector<ui::LatencyInfo> software_latency_info_;

  scoped_ptr<aura::client::ScopedTooltipDisabler> tooltip_disabler_;

  // True when this view acts as a platform view hack for a
  // RenderWidgetHostViewGuest.
  bool is_guest_view_hack_;

  base::WeakPtrFactory<RenderWidgetHostViewAura> weak_ptr_factory_;

  gfx::Rect disambiguation_target_rect_;

  // The last scroll offset when we start to render the link disambiguation
  // view, so we can ensure the window hasn't moved between copying from the
  // compositing surface and showing the disambiguation popup.
  gfx::Vector2dF disambiguation_scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
