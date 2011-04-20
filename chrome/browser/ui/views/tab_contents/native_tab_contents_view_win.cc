// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_win.h"

#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents/web_drop_target_win.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_drag_win.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"

namespace {

// Tabs must be created as child widgets, otherwise they will be given
// a FocusManager which will conflict with the FocusManager of the
// window they eventually end up attached to.
//
// A tab will not have a parent HWND whenever it is not active in its
// host window - for example at creation time and when it's in the
// background, so we provide a default widget to host them.
//
// It may be tempting to use GetDesktopWindow() instead, but this is
// problematic as the shell sends messages to children of the desktop
// window that interact poorly with us.
//
// See: http://crbug.com/16476
HWND GetHiddenTabHostWindow() {
  static views::Widget* widget = NULL;

  if (!widget) {
    views::Widget::CreateParams params(views::Widget::CreateParams::TYPE_POPUP);
    widget = views::Widget::CreateWidget(params);
    widget->Init(NULL, gfx::Rect());
    // If a background window requests focus, the hidden tab host will
    // be activated to focus the tab.  Use WS_DISABLED to prevent
    // this.
    EnableWindow(widget->GetNativeView(), FALSE);
  }

  return widget->GetNativeView();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, public:

NativeTabContentsViewWin::NativeTabContentsViewWin(
    internal::NativeTabContentsViewDelegate* delegate)
    : delegate_(delegate),
      focus_manager_(NULL) {
}

NativeTabContentsViewWin::~NativeTabContentsViewWin() {
  CloseNow();
}

TabContents* NativeTabContentsViewWin::GetTabContents() const {
  return delegate_->GetTabContents();
}

void NativeTabContentsViewWin::EndDragging() {
  delegate_->OnNativeTabContentsViewDraggingEnded();
  drag_handler_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, NativeTabContentsView implementation:

void NativeTabContentsViewWin::InitNativeTabContentsView() {
  views::Widget::CreateParams params(views::Widget::CreateParams::TYPE_CONTROL);
  params.delete_on_destroy = false;
  SetCreateParams(params);
  WidgetWin::Init(GetHiddenTabHostWindow(), gfx::Rect());

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetNativeView());
  drop_target_ = new WebDropTarget(GetNativeView(),
                                   delegate_->GetTabContents());
}

void NativeTabContentsViewWin::Unparent() {
  // Remember who our FocusManager is, we won't be able to access it once
  // unparented.
  focus_manager_ = views::WidgetWin::GetFocusManager();
  // Note that we do not DCHECK on focus_manager_ as it may be NULL when used
  // with an external tab container.
  NativeWidget::ReparentNativeView(GetNativeView(), GetHiddenTabHostWindow());
}

RenderWidgetHostView* NativeTabContentsViewWin::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewWin* view =
      new RenderWidgetHostViewWin(render_widget_host);
  view->CreateWnd(GetNativeView());
  view->ShowWindow(SW_SHOW);
  return view;
}

gfx::NativeWindow NativeTabContentsViewWin::GetTopLevelNativeWindow() const {
  return ::GetAncestor(GetNativeView(), GA_ROOT);
}

void NativeTabContentsViewWin::SetPageTitle(const std::wstring& title) {
  // It's possible to get this after the hwnd has been destroyed.
  if (GetNativeView())
    ::SetWindowText(GetNativeView(), title.c_str());
}

void NativeTabContentsViewWin::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  drag_handler_ = new TabContentsDragWin(this);
  drag_handler_->StartDragging(drop_data, ops, image, image_offset);
}

void NativeTabContentsViewWin::CancelDrag() {
  drag_handler_->CancelDrag();
}

bool NativeTabContentsViewWin::IsDoingDrag() const {
  return drag_handler_.get() != NULL;
}

void NativeTabContentsViewWin::SetDragCursor(
    WebKit::WebDragOperation operation) {
  drop_target_->set_drag_cursor(operation);
}

views::NativeWidget* NativeTabContentsViewWin::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, views::WidgetWin overrides:

void NativeTabContentsViewWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(GetNativeView());
    drop_target_ = NULL;
  }

  WidgetWin::OnDestroy();
}

void NativeTabContentsViewWin::OnHScroll(int scroll_type,
                                         short position,
                                         HWND scrollbar) {
  ScrollCommon(WM_HSCROLL, scroll_type, position, scrollbar);
}

LRESULT NativeTabContentsViewWin::OnMouseRange(UINT msg,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  if (delegate_->IsShowingSadTab())
    return WidgetWin::OnMouseRange(msg, w_param, l_param);

  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      delegate_->OnNativeTabContentsViewMouseDown();
      break;
    }
    case WM_MOUSEMOVE:
      delegate_->OnNativeTabContentsViewMouseMove();
      break;
    default:
      break;
  }
  return 0;
}

// A message is reflected here from view().
// Return non-zero to indicate that it is handled here.
// Return 0 to allow view() to further process it.
LRESULT NativeTabContentsViewWin::OnReflectedMessage(UINT msg,
                                                     WPARAM w_param,
                                                     LPARAM l_param) {
  MSG* message = reinterpret_cast<MSG*>(l_param);
  switch (message->message) {
    case WM_MOUSEWHEEL:
      // This message is reflected from the view() to this window.
      if (GET_KEYSTATE_WPARAM(message->wParam) & MK_CONTROL) {
        delegate_->OnNativeTabContentsViewWheelZoom(
            GET_WHEEL_DELTA_WPARAM(message->wParam));
        return 1;
      }
      break;
    case WM_HSCROLL:
    case WM_VSCROLL:
      if (ScrollZoom(LOWORD(message->wParam)))
        return 1;
    default:
      break;
  }

  return 0;
}

void NativeTabContentsViewWin::OnVScroll(int scroll_type,
                                         short position,
                                         HWND scrollbar) {
  ScrollCommon(WM_VSCROLL, scroll_type, position, scrollbar);
}

void NativeTabContentsViewWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (window_pos->flags & SWP_HIDEWINDOW) {
    delegate_->OnNativeTabContentsViewHidden();
  } else {
    // The TabContents was shown by a means other than the user selecting a
    // Tab, e.g. the window was minimized then restored.
    if (window_pos->flags & SWP_SHOWWINDOW)
      delegate_->OnNativeTabContentsViewShown();

    // Unless we were specifically told not to size, cause the renderer to be
    // sized to the new bounds, which forces a repaint. Not required for the
    // simple minimize-restore case described above, for example, since the
    // size hasn't changed.
    if (!(window_pos->flags & SWP_NOSIZE)) {
      delegate_->OnNativeTabContentsViewSized(
          gfx::Size(window_pos->cx, window_pos->cy));
    }
  }
  WidgetWin::OnWindowPosChanged(window_pos);
}

void NativeTabContentsViewWin::OnSize(UINT param, const CSize& size) {
  // NOTE: Because TabContentsViewViews handles OnWindowPosChanged without
  // calling DefWindowProc, OnSize is NOT called on window resize. This handler
  // is called only once when the window is created.

  // Don't call base class OnSize to avoid useless layout for 0x0 size.
  // We will get OnWindowPosChanged later and layout root view in WasSized.

  // Hack for ThinkPad touch-pad driver.
  // Set fake scrollbars so that we can get scroll messages,
  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_ALL;

  si.nMin = 1;
  si.nMax = 100;
  si.nPage = 10;
  si.nPos = 50;

  ::SetScrollInfo(GetNativeView(), SB_HORZ, &si, FALSE);
  ::SetScrollInfo(GetNativeView(), SB_VERT, &si, FALSE);
}

LRESULT NativeTabContentsViewWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  // Hack for ThinkPad mouse wheel driver. We have set the fake scroll bars
  // to receive scroll messages from ThinkPad touch-pad driver. Suppress
  // painting of scrollbars by returning 0 size for them.
  return 0;
}

void NativeTabContentsViewWin::OnNCPaint(HRGN rgn) {
  // Suppress default WM_NCPAINT handling. We don't need to do anything
  // here since the view will draw everything correctly.
}

views::FocusManager* NativeTabContentsViewWin::GetFocusManager() {
  views::FocusManager* focus_manager = WidgetWin::GetFocusManager();
  if (focus_manager) {
    // If focus_manager_ is non NULL, it means we have been reparented, in which
    // case its value may not be valid anymore.
    focus_manager_ = NULL;
    return focus_manager;
  }
  // TODO(jcampan): we should DCHECK on focus_manager_, as it should not be
  // NULL.  We are not doing it as it breaks some unit-tests.  We should
  // probably have an empty TabContentView implementation for the unit-tests,
  // that would prevent that code being executed in the unit-test case.
  // DCHECK(focus_manager_);
  return focus_manager_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, private:

void NativeTabContentsViewWin::ScrollCommon(UINT message, int scroll_type,
                                            short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // touch-pad scroll wheel emulation.
  if (!ScrollZoom(scroll_type)) {
    // Reflect scroll message to the view() to give it a chance
    // to process scrolling.
    SendMessage(delegate_->GetTabContents()->view()->GetContentNativeView(),
                message, MAKELONG(scroll_type, position),
                reinterpret_cast<LPARAM>(scrollbar));
  }
}

bool NativeTabContentsViewWin::ScrollZoom(int scroll_type) {
  // If ctrl is held, zoom the UI.  There are three issues with this:
  // 1) Should the event be eaten or forwarded to content?  We eat the event,
  //    which is like Firefox and unlike IE.
  // 2) Should wheel up zoom in or out?  We zoom in (increase font size), which
  //    is like IE and Google maps, but unlike Firefox.
  // 3) Should the mouse have to be over the content area?  We zoom as long as
  //    content has focus, although FF and IE require that the mouse is over
  //    content.  This is because all events get forwarded when content has
  //    focus.
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
    int distance = 0;
    switch (scroll_type) {
      case SB_LINEUP:
        distance = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:
        distance = -WHEEL_DELTA;
        break;
        // TODO(joshia): Handle SB_PAGEUP, SB_PAGEDOWN, SB_THUMBPOSITION,
        // and SB_THUMBTRACK for completeness
      default:
        break;
    }

    delegate_->OnNativeTabContentsViewWheelZoom(distance);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewWin(delegate);
}
