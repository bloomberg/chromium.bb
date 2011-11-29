// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/win/scoped_comptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/win/ime_input.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/surface/accelerated_surface_win.h"
#include "webkit/glue/webcursor.h"

class BackingStore;
class RenderWidgetHost;

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

typedef CWinTraits<WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0>
    RenderWidgetHostHWNDTraits;

CONTENT_EXPORT extern const wchar_t kRenderWidgetHostHWNDClass[];

// TODO(ananta)
// This should be removed once we have the new windows SDK which defines these
// messages.
#if !defined(WM_POINTERUPDATE)
#define WM_POINTERUPDATE 0x0245
#endif  // WM_POINTERUPDATE

#if !defined(WM_POINTERDOWN)
#define WM_POINTERDOWN  0x0246
#endif  // WM_POINTERDOWN

#if !defined(WM_POINTERUP)
#define WM_POINTERUP    0x0247
#endif  // WM_POINTERUP

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
class RenderWidgetHostViewWin
    : public CWindowImpl<RenderWidgetHostViewWin,
                         CWindow,
                         RenderWidgetHostHWNDTraits>,
      public RenderWidgetHostView,
      public content::NotificationObserver,
      public BrowserAccessibilityDelegate {
 public:
  // The view will associate itself with the given widget.
  CONTENT_EXPORT explicit RenderWidgetHostViewWin(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewWin();

  CONTENT_EXPORT void CreateWnd(HWND parent);

  void ScheduleComposite();

  CONTENT_EXPORT IAccessible* GetIAccessible();

  DECLARE_WND_CLASS_EX(kRenderWidgetHostHWNDClass, CS_DBLCLKS, 0);

  BEGIN_MSG_MAP(RenderWidgetHostHWND)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_NCPAINT(OnNCPaint)
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

  // Implementation of RenderWidgetHostView:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void DidBecomeSelected() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(ui::TextInputType type,
                                     bool can_compose_inline) OVERRIDE;
  virtual void SelectionBoundsChanged(const gfx::Rect& start_rect,
                                      const gfx::Rect& end_rect) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void ImeCompositionRangeChanged(const ui::Range& range) OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  // called by TabContents before DestroyWindow
  virtual void WillWmDestroy() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual void SetVisuallyDeemphasized(const SkColor* color,
                                       bool animate) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual gfx::PluginWindowHandle GetCompositingSurface() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void OnAccessibilityNotifications(
      const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params
      ) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

  // Implementation of content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Implementation of BrowserAccessibilityDelegate:
  virtual void SetAccessibilityFocus(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) OVERRIDE;

 protected:
  // Windows Message Handlers
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnActivate(UINT, BOOL, HWND);
  void OnDestroy();
  void OnPaint(HDC unused_dc);
  void OnNCPaint(HRGN update_region);
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

  // Create an intermediate window between the given HWND and its parent.
  HWND ReparentWindow(HWND window);

  // Clean up the compositor window, if needed.
  void CleanupCompositorWindow();

  // Whether the window should be activated.
  bool IsActivatable() const;

  // Do initialization needed by both InitAsPopup() and InitAsFullscreen().
  void DoPopupOrFullscreenInit(HWND parent_hwnd,
                               const gfx::Rect& pos,
                               DWORD ex_style);

  CPoint GetClientCenter() const;
  void MoveCursorToCenter() const;

  void HandleLockedMouseEvent(UINT message, WPARAM wparam, LPARAM lparam);

  LRESULT OnDocumentFeed(RECONVERTSTRING* reconv);
  LRESULT OnReconvertString(RECONVERTSTRING* reconv);

  // The associated Model.  While |this| is being Destroyed,
  // |render_widget_host_| is NULL and the Windows message loop is run one last
  // time. Message handlers must check for a NULL |render_widget_host_|.
  RenderWidgetHost* render_widget_host_;

  // When we are doing accelerated compositing
  HWND compositor_host_window_;

  // Presents a texture received from another process to the compositing
  // window.
  scoped_refptr<AcceleratedSurface> accelerated_surface_;

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

  // Wrapper for maintaining touchstate associated with a WebTouchEvent.
  class WebTouchState {
   public:
    explicit WebTouchState(const CWindowImpl* window);

    // Updates the current touchpoint state with the supplied touches.
    // Touches will be consumed only if they are of the same type (e.g. down,
    // up, move). Returns the number of consumed touches.
    size_t UpdateTouchPoints(TOUCHINPUT* points, size_t count);

    // Marks all active touchpoints as released.
    bool ReleaseTouchPoints();

    // The contained WebTouchEvent.
    const WebKit::WebTouchEvent& touch_event() { return touch_event_; }

    // Returns if any touches are modified in the event.
    bool is_changed() { return touch_event_.changedTouchesLength != 0; }

   private:
    // Adds a touch point or returns NULL if there's not enough space.
    WebKit::WebTouchPoint* AddTouchPoint(TOUCHINPUT* touch_input);

    // Copy details from a TOUCHINPUT to an existing WebTouchPoint, returning
    // true if the resulting point is a stationary move.
    bool UpdateTouchPoint(WebKit::WebTouchPoint* touch_point,
                          TOUCHINPUT* touch_input);

    WebKit::WebTouchEvent touch_event_;
    const CWindowImpl* const window_;
  };

  // The touch-state. Its touch-points are updated as necessary. A new
  // touch-point is added from an TOUCHEVENTF_DOWN message, and a touch-point
  // is removed from the list on an TOUCHEVENTF_UP message.
  WebTouchState touch_state_;

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

  // Our parent HWND.  We keep a reference to it as we SetParent(NULL) when
  // hidden to prevent getting messages (Paint, Resize...), and we reattach
  // when shown again.
  HWND parent_hwnd_;

  // Instance of accessibility information for the root of the MSAA
  // tree representation of the WebKit render tree.
  scoped_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks tab_switch_paint_time_;

  // A color we use to shade the entire render view. If 100% transparent, we do
  // not shade the render view.
  SkColor overlay_color_;

  // Registrar so we can listen to RENDERER_PROCESS_TERMINATED events.
  content::NotificationRegistrar registrar_;

  // Stores the current text input type received by TextInputStateChanged()
  // method.
  ui::TextInputType text_input_type_;

  ScopedVector<ui::ViewProp> props_;

  // Is the widget fullscreen?
  bool is_fullscreen_;

  // Used to record the last position of the mouse.
  // While the mouse is locked, they store the last known position just as mouse
  // lock was entered.
  // Relative to the upper-left corner of the view.
  gfx::Point last_mouse_position_;
  // Relative to the upper-left corner of the screen.
  gfx::Point last_global_mouse_position_;

  // In the case of the mouse being moved away from the view and then moved
  // back, we regard the mouse movement as (0, 0).
  bool ignore_mouse_movement_;

  ui::Range composition_range_;

  // Set to true if the next lbutton down message is to be ignored. Set by the
  // WM_POINTERXX handler. We do this to ensure that we don't send out
  // duplicate lbutton down messages to the renderer.
  bool ignore_next_lbutton_message_at_same_location;
  // The location of the last WM_POINTERDOWN message. We ignore the subsequent
  // lbutton down only if the locations match.
  LPARAM last_pointer_down_location_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewWin);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_WIN_H_
