// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_win.h"

#include "base/bind.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/tab_contents/web_drag_bookmark_handler_win.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/web_contents_drag_win.h"
#include "content/browser/web_contents/web_drag_dest_win.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

using content::RenderWidgetHostView;
using content::WebContents;

namespace {

// See comment above TempParent in tab_contents_view_win.cc.
// Also, Tabs must be created as child widgets, otherwise they will be given
// a FocusManager which will conflict with the FocusManager of the
// window they eventually end up attached to.
class HiddenTabHostWindow : public views::Widget {
 public:
  static HWND Instance();

 private:
  HiddenTabHostWindow() {}

  ~HiddenTabHostWindow() {
    instance_ = NULL;
  }

  // Do nothing when asked to close.  External applications (notably
  // installers) try to close Chrome by sending WM_CLOSE to Chrome's
  // top level windows.  We don't want the tab host to close due to
  // those messages.  Instead the browser will close when the browser
  // window receives a WM_CLOSE.
  virtual void Close() OVERRIDE {}

  static HiddenTabHostWindow* instance_;

  DISALLOW_COPY_AND_ASSIGN(HiddenTabHostWindow);
};

HiddenTabHostWindow* HiddenTabHostWindow::instance_ = NULL;

HWND HiddenTabHostWindow::Instance() {
  if (instance_ == NULL) {
    instance_ = new HiddenTabHostWindow;
    // We don't want this widget to be closed automatically, this causes
    // problems in tests that close the last non-secondary window.
    instance_->set_is_secondary_widget(false);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    instance_->Init(params);
    // If a background window requests focus, the hidden tab host will
    // be activated to focus the tab.  Disable the window to prevent
    // this.
    EnableWindow(instance_->GetNativeView(), FALSE);
  }

  return instance_->GetNativeView();
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, public:

NativeTabContentsViewWin::NativeTabContentsViewWin(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::NativeWidgetWin(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

NativeTabContentsViewWin::~NativeTabContentsViewWin() {
}

WebContents* NativeTabContentsViewWin::GetWebContents() const {
  return delegate_->GetWebContents();
}

void NativeTabContentsViewWin::EndDragging() {
  delegate_->OnNativeTabContentsViewDraggingEnded();
  drag_handler_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, NativeTabContentsView implementation:

void NativeTabContentsViewWin::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = HiddenTabHostWindow::Instance();
  GetWidget()->Init(params);

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetNativeView());
  drag_dest_ = new WebDragDest(GetNativeView(), delegate_->GetWebContents());
  bookmark_handler_.reset(new WebDragBookmarkHandlerWin());
  drag_dest_->set_delegate(bookmark_handler_.get());
}

RenderWidgetHostView* NativeTabContentsViewWin::CreateRenderWidgetHostView(
    content::RenderWidgetHost* render_widget_host) {
  RenderWidgetHostView* view =
      RenderWidgetHostView::CreateViewForWidget(render_widget_host);

  view->InitAsChild(GetNativeView());
  view->Show();
  return view;
}

gfx::NativeWindow NativeTabContentsViewWin::GetTopLevelNativeWindow() const {
  return ::GetAncestor(GetNativeView(), GA_ROOT);
}

void NativeTabContentsViewWin::SetPageTitle(const string16& title) {
  // It's possible to get this after the hwnd has been destroyed.
  if (GetNativeView())
    ::SetWindowText(GetNativeView(), title.c_str());
}

void NativeTabContentsViewWin::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  drag_handler_ = new WebContentsDragWin(
      GetNativeView(),
      delegate_->GetWebContents(),
      drag_dest_,
      base::Bind(&NativeTabContentsViewWin::EndDragging,
                 base::Unretained(this)));
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
  drag_dest_->set_drag_cursor(operation);
}

views::NativeWidget* NativeTabContentsViewWin::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, views::NativeWidgetWin overrides:

void NativeTabContentsViewWin::OnDestroy() {
  if (drag_dest_.get()) {
    RevokeDragDrop(GetNativeView());
    drag_dest_ = NULL;
  }

  NativeWidgetWin::OnDestroy();
}

void NativeTabContentsViewWin::OnHScroll(int scroll_type,
                                         short position,
                                         HWND scrollbar) {
  ScrollCommon(WM_HSCROLL, scroll_type, position, scrollbar);
}

LRESULT NativeTabContentsViewWin::OnMouseRange(UINT msg,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(GetWebContents());
  if (wrapper && wrapper->sad_tab_helper()->HasSadTab())
    return NativeWidgetWin::OnMouseRange(msg, w_param, l_param);

  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      delegate_->OnNativeTabContentsViewMouseDown();
      break;
    }
    case WM_MOUSEMOVE:
      delegate_->OnNativeTabContentsViewMouseMove(true);
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
            GET_WHEEL_DELTA_WPARAM(message->wParam) > 0);
        return 1;
      }
      break;
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
  NativeWidgetWin::OnWindowPosChanged(window_pos);
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

LRESULT NativeTabContentsViewWin::OnNCHitTest(const CPoint& point) {
  // Allow hit tests to fall through to the parent window.
  SetMsgHandled(true);
  return HTTRANSPARENT;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewWin, private:

void NativeTabContentsViewWin::ScrollCommon(UINT message, int scroll_type,
                                            short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // touch-pad scroll wheel emulation.
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

    delegate_->OnNativeTabContentsViewWheelZoom(distance > 0);
    return;
  }

  // Reflect scroll message to the view() to give it a chance
  // to process scrolling.
  SendMessage(delegate_->GetWebContents()->GetView()->GetContentNativeView(),
              message, MAKELONG(scroll_type, position),
              reinterpret_cast<LPARAM>(scrollbar));
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewWin(delegate);
}
