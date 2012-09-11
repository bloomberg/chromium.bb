// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_WIN_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <peninputpanel.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/win/scoped_comptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/win/extra_sdk_defines.h"
#include "ui/base/win/ime_input.h"
#include "ui/base/win/tsf_bridge.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/surface/accelerated_surface_win.h"
#include "webkit/glue/webcursor.h"

class SkRegion;

namespace gfx {
class Size;
class Rect;
}

namespace IPC {
class Message;
}

namespace ui {
class ViewProp;
}

namespace content {
class BackingStore;
class RenderWidgetHost;
class WebTouchState;

typedef CWinTraits<WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0>
    RenderWidgetHostHWNDTraits;

CONTENT_EXPORT extern const wchar_t kRenderWidgetHostHWNDClass[];

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewWin
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, receiving windows
//  messages, and containing plugins HWNDs. It is the implementation of the
//  RenderWidgetHostView that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHostHWND is tied to the render process.
//     If the render process dies, the RenderWidgetHostHWND goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class RenderWidgetHostViewWin
    : public CWindowImpl<RenderWidgetHostViewWin,
                         CWindow,
                         RenderWidgetHostHWNDTraits>,
      public RenderWidgetHostViewBase,
      public NotificationObserver,
      public BrowserAccessibilityDelegate,
      public ui::GestureConsumer,
      public ui::GestureEventHelper,
      public ui::TextInputClient {  // for Win8/metro TSF support.
 public:
  virtual ~RenderWidgetHostViewWin();

  CONTENT_EXPORT void CreateWnd(HWND parent);

  void AcceleratedPaint(HDC dc);

  DECLARE_WND_CLASS_EX(kRenderWidgetHostHWNDClass, CS_DBLCLKS, 0);

  BEGIN_MSG_MAP(RenderWidgetHostHWND)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_NCPAINT(OnNCPaint)
    MSG_WM_NCHITTEST(OnNCHitTest)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_SETCURSOR(OnSetCursor)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_KILLFOCUS(OnKillFocus)
    MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    MSG_WM_CANCELMODE(OnCancelMode)
    MSG_WM_INPUTLANGCHANGE(OnInputLangChange)
    MSG_WM_THEMECHANGED(OnThemeChanged)
    MSG_WM_NOTIFY(OnNotify)
    MESSAGE_HANDLER(WM_IME_SETCONTEXT, OnImeSetContext)
    MESSAGE_HANDLER(WM_IME_STARTCOMPOSITION, OnImeStartComposition)
    MESSAGE_HANDLER(WM_IME_COMPOSITION, OnImeComposition)
    MESSAGE_HANDLER(WM_IME_ENDCOMPOSITION, OnImeEndComposition)
    MESSAGE_HANDLER(WM_IME_REQUEST, OnImeRequest)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseEvent)
    MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseEvent)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnMouseEvent)
    MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMouseEvent)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnMouseEvent)
    MESSAGE_HANDLER(WM_LBUTTONUP, OnMouseEvent)
    MESSAGE_HANDLER(WM_MBUTTONUP, OnMouseEvent)
    MESSAGE_HANDLER(WM_RBUTTONUP, OnMouseEvent)
    MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnMouseEvent)
    MESSAGE_HANDLER(WM_MBUTTONDBLCLK, OnMouseEvent)
    MESSAGE_HANDLER(WM_RBUTTONDBLCLK, OnMouseEvent)
    MESSAGE_HANDLER(WM_SYSKEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER(WM_SYSKEYUP, OnKeyEvent)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER(WM_KEYUP, OnKeyEvent)
    MESSAGE_HANDLER(WM_MOUSEWHEEL, OnWheelEvent)
    MESSAGE_HANDLER(WM_MOUSEHWHEEL, OnWheelEvent)
    MESSAGE_HANDLER(WM_HSCROLL, OnWheelEvent)
    MESSAGE_HANDLER(WM_VSCROLL, OnWheelEvent)
    MESSAGE_HANDLER(WM_CHAR, OnKeyEvent)
    MESSAGE_HANDLER(WM_SYSCHAR, OnKeyEvent)
    MESSAGE_HANDLER(WM_TOUCH, OnTouchEvent)
    MESSAGE_HANDLER(WM_IME_CHAR, OnKeyEvent)
    MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
    MESSAGE_HANDLER(WM_GETOBJECT, OnGetObject)
    MESSAGE_HANDLER(WM_PARENTNOTIFY, OnParentNotify)
    MESSAGE_HANDLER(WM_POINTERDOWN, OnPointerMessage)
    MESSAGE_HANDLER(WM_POINTERUP, OnPointerMessage)
    MESSAGE_HANDLER(WM_GESTURE, OnGestureEvent)
  END_MSG_MAP()

  // RenderWidgetHostView implementation.
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;

  // Implementation of RenderWidgetHostViewPort.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Point& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const gfx::Rect& start_rect,
      WebKit::WebTextDirection start_direction,
      const gfx::Rect& end_rect,
      WebKit::WebTextDirection end_direction) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void ImeCompositionRangeChanged(
      const ui::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  // called by WebContentsImpl before DestroyWindow
  virtual void WillWmDestroy() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool)>& callback,
      skia::PlatformCanvas* output) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void ProcessTouchAck(WebKit::WebInputEvent::Type type,
                               bool processed) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>& params
      ) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void SetClickthroughRegion(SkRegion* region) OVERRIDE;

  // Implementation of NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Implementation of BrowserAccessibilityDelegate:
  virtual void SetAccessibilityFocus(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, gfx::Rect subfocus) OVERRIDE;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, gfx::Point point) OVERRIDE;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) OVERRIDE;

  // Overridden from ui::GestureEventHelper.
  virtual bool DispatchLongPressGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual bool DispatchCancelTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from ui::TextInputClient for Win8/metro TSF support.
  // Following methods are not used in existing IMM32 related implementation.
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText()  OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() OVERRIDE;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) OVERRIDE;
  virtual bool HasCompositionText() OVERRIDE;
  virtual bool GetTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetCompositionTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetSelectionRange(ui::Range* range) OVERRIDE;
  virtual bool SetSelectionRange(const ui::Range& range) OVERRIDE;
  virtual bool DeleteRange(const ui::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const ui::Range& range,
                                string16* text) OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;

 protected:
  friend class RenderWidgetHostView;

  // Should construct only via RenderWidgetHostView::CreateViewForWidget.
  //
  // The view will associate itself with the given widget.
  explicit RenderWidgetHostViewWin(RenderWidgetHost* widget);

  // Windows Message Handlers
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnActivate(UINT, BOOL, HWND);
  void OnDestroy();
  void OnPaint(HDC unused_dc);
  void OnNCPaint(HRGN update_region);
  LRESULT OnNCHitTest(const CPoint& pt);
  LRESULT OnEraseBkgnd(HDC dc);
  LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT mouse_message_id);
  void OnSetFocus(HWND window);
  void OnKillFocus(HWND window);
  void OnCaptureChanged(HWND window);
  void OnCancelMode();
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  void OnThemeChanged();
  LRESULT OnNotify(int w_param, NMHDR* header);
  LRESULT OnImeSetContext(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeStartComposition(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeComposition(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeEndComposition(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeRequest(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnMouseEvent(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnKeyEvent(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnWheelEvent(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnTouchEvent(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnMouseActivate(UINT message,
                          WPARAM wparam,
                          LPARAM lparam,
                          BOOL& handled);
  // Handle MSAA requests for accessibility information.
  LRESULT OnGetObject(UINT message, WPARAM wparam, LPARAM lparam,
                      BOOL& handled);
  // Handle vertical scrolling.
  LRESULT OnVScroll(int code, short position, HWND scrollbar_control);
  // Handle horizontal scrolling.
  LRESULT OnHScroll(int code, short position, HWND scrollbar_control);

  LRESULT OnParentNotify(UINT message, WPARAM wparam, LPARAM lparam,
                         BOOL& handled);

  // Handle the new pointer messages
  LRESULT OnPointerMessage(UINT message, WPARAM wparam, LPARAM lparam,
                           BOOL& handled);
  // Handle high-level touch events.
  LRESULT OnGestureEvent(UINT message, WPARAM wparam, LPARAM lparam,
                         BOOL& handled);

  void OnFinalMessage(HWND window);

 private:
  // Updates the display cursor to the current cursor if the cursor is over this
  // render view.
  void UpdateCursorIfOverSelf();

  // Tells Windows that we want to hear about mouse exit messages.
  void TrackMouseLeave(bool start_tracking);

  // Sends a message to the RenderView in the renderer process.
  bool Send(IPC::Message* message);

  // Set the tooltip region to the size of the window, creating the tooltip
  // hwnd if it has not been created yet.
  void EnsureTooltip();

  // Tooltips become invalid when the root ancestor changes. When the View
  // becomes hidden, this method is called to reset the tooltip.
  void ResetTooltip();

  // Builds and forwards a WebKitGestureEvent to the renderer.
  bool ForwardGestureEventToRenderer(
    ui::GestureEvent* gesture);

  // Process all of the given gestures (passes them on to renderer)
  void ProcessGestures(ui::GestureRecognizer::Gestures* gestures);

  // Sends the specified mouse event to the renderer.
  void ForwardMouseEventToRenderer(UINT message, WPARAM wparam, LPARAM lparam);

  // Synthesize mouse wheel event.
  LRESULT SynthesizeMouseWheel(bool is_vertical, int scroll_code,
                               short scroll_position);

  // Shuts down the render_widget_host_.  This is a separate function so we can
  // invoke it from the message loop.
  void ShutdownHost();

  // Redraws the window synchronously, and any child windows (i.e. plugins)
  // asynchronously.
  void Redraw();

  // Draw our background over the given HDC in the given |rect|. The background
  // will be tiled such that it lines up with existing tiles starting from the
  // origin of |dc|.
  void DrawBackground(const RECT& rect, CPaintDC* dc);

  // Clean up the compositor window, if needed.
  void CleanupCompositorWindow();

  // Whether the window should be activated.
  bool IsActivatable() const;

  // Do initialization needed by both InitAsPopup() and InitAsFullscreen().
  void DoPopupOrFullscreenInit(HWND parent_hwnd,
                               const gfx::Rect& pos,
                               DWORD ex_style);

  CPoint GetClientCenter() const;
  // In mouse lock mode, moves the mouse cursor to the center of the view if it
  // is too close to the border.
  void MoveCursorToCenterIfNecessary();

  void HandleLockedMouseEvent(UINT message, WPARAM wparam, LPARAM lparam);

  LRESULT OnDocumentFeed(RECONVERTSTRING* reconv);
  LRESULT OnReconvertString(RECONVERTSTRING* reconv);
  LRESULT OnQueryCharPosition(IMECHARPOSITION* position);

  // Displays the on screen keyboard for editable fields.
  void DisplayOnScreenKeyboardIfNeeded();

  // Invoked in a delayed task to reset the fact that we are in the context of
  // a WM_POINTERDOWN message.
  void ResetPointerDownContext();

  // Switches between raw-touches mode and gesture mode. Currently touch mode
  // will only take effect when kEnableTouchEvents is in effect.
  void UpdateDesiredTouchMode(bool touch);

  // Set window to receive gestures.
  void SetToGestureMode();

  // Set window to raw touch events. Returns whether registering was successful.
  bool SetToTouchMode();

  // Configures the enable/disable state of |ime_input_| to match with the
  // current |text_input_type_|.
  void UpdateIMEState();

  // The associated Model.  While |this| is being Destroyed,
  // |render_widget_host_| is NULL and the Windows message loop is run one last
  // time. Message handlers must check for a NULL |render_widget_host_|.
  RenderWidgetHostImpl* render_widget_host_;

  // When we are doing accelerated compositing
  HWND compositor_host_window_;

  // Presents a texture received from another process to the compositing
  // window.
  scoped_ptr<AcceleratedSurface> accelerated_surface_;

  // true if the compositor host window must be hidden after the
  // software renderered view is updated.
  bool hide_compositor_window_at_next_paint_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Indicates if the page is loading.
  bool is_loading_;

  // true if we are currently tracking WM_MOUSEEXIT messages.
  bool track_mouse_leave_;

  // Wrapper class for IME input.
  // (See "ui/base/win/ime_input.h" for its details.)
  ui::ImeInput ime_input_;

  // Represents whether or not this browser process is receiving status
  // messages about the focused edit control from a renderer process.
  bool ime_notification_;

  // true if Enter was hit when render widget host was in focus.
  bool capture_enter_key_;

  // true if the View is not visible.
  bool is_hidden_;

  // The touch-state. Its touch-points are updated as necessary. A new
  // touch-point is added from an TOUCHEVENTF_DOWN message, and a touch-point
  // is removed from the list on an TOUCHEVENTF_UP message.
  scoped_ptr<WebTouchState> touch_state_;

  // True if we're in the midst of a paint operation and should respond to
  // DidPaintRect() notifications by merely invalidating.  See comments on
  // render_widget_host_view.h:DidPaintRect().
  bool about_to_validate_and_paint_;

  // true if the View should be closed when its HWND is deactivated (used to
  // support SELECT popups which are closed when they are deactivated).
  bool close_on_deactivate_;

  // Whether Destroy() has been called.  Used to detect a crasher
  // (http://crbug.com/24248) where render_view_host_ has been deleted when
  // OnFinalMessage is called.
  bool being_destroyed_;

  // Tooltips
  // The text to be shown in the tooltip, supplied by the renderer.
  string16 tooltip_text_;
  // The tooltip control hwnd
  HWND tooltip_hwnd_;
  // Whether or not a tooltip is currently visible. We use this to track
  // whether or not we want to force-close the tooltip when we receive mouse
  // move notifications from the renderer. See comment in OnMsgSetTooltipText.
  bool tooltip_showing_;

  // Factory used to safely scope delayed calls to ShutdownHost().
  base::WeakPtrFactory<RenderWidgetHostViewWin> weak_factory_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks web_contents_switch_paint_time_;

  // Registrar so we can listen to RENDERER_PROCESS_TERMINATED events.
  NotificationRegistrar registrar_;

  // Stores the current text input type received by TextInputStateChanged()
  // method.
  ui::TextInputType text_input_type_;

  ScopedVector<ui::ViewProp> props_;

  // Is the widget fullscreen?
  bool is_fullscreen_;

  // Used to record the last position of the mouse.
  struct {
    // While the mouse is locked, |unlocked| and |unlocked_global| store the
    // last known position just as mouse lock was entered.
    // Relative to the upper-left corner of the view.
    gfx::Point unlocked;
    // Relative to the upper-left corner of the screen.
    gfx::Point unlocked_global;

    // Only valid while the mouse is locked.
    gfx::Point locked_global;
  } last_mouse_position_;

  // When the mouse cursor is moved to the center of the view by
  // MoveCursorToCenterIfNecessary(), we ignore the resulting WM_MOUSEMOVE
  // message.
  struct {
    bool pending;
    // Relative to the upper-left corner of the screen.
    gfx::Point target;
  } move_to_center_request_;

  // In the case of the mouse being moved away from the view and then moved
  // back, we regard the mouse movement as (0, 0).
  bool ignore_mouse_movement_;

  ui::Range composition_range_;

  // The current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // A cached latest caret rectangle sent from renderer.
  gfx::Rect caret_rect_;

  // TODO(ananta)
  // The WM_POINTERDOWN and on screen keyboard handling related members should
  // be moved to an independent class to reduce the clutter. This includes all
  // members starting from virtual_keyboard_ to
  // received_focus_change_after_pointer_down_.

  // ITextInputPanel to allow us to show the Windows virtual keyboard when a
  // user touches an editable field on the page.
  base::win::ScopedComPtr<ITextInputPanel> virtual_keyboard_;

  // Set to true if we are in the context of a WM_POINTERDOWN message
  bool pointer_down_context_;

  // Set to true if the focus is currently on an editable field on the page.
  bool focus_on_editable_field_;

  // Set to true if we received a focus change after a WM_POINTERDOWN message.
  bool received_focus_change_after_pointer_down_;

  // Region in which the view will be transparent to clicks.
  scoped_ptr<SkRegion> transparent_region_;

  // Are touch events currently enabled?
  bool touch_events_enabled_;

  scoped_ptr<ui::GestureRecognizer> gesture_recognizer_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_WIN_H_
