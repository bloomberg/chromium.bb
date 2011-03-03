// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/tab_contents_view_win.h"

#include <windows.h>

#include <vector>

#include "base/time.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents/web_drop_target_win.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_drag_win.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "views/focus/view_storage.h"
#include "views/screen.h"
#include "views/widget/root_view.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewWin(tab_contents);
}

TabContentsViewWin::TabContentsViewWin(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      focus_manager_(NULL),
      close_tab_after_drag_ends_(false),
      sad_tab_(NULL) {
  last_focused_view_storage_id_ =
      views::ViewStorage::GetInstance()->CreateStorageID();
}

TabContentsViewWin::~TabContentsViewWin() {
  // Makes sure to remove any stored view we may still have in the ViewStorage.
  //
  // It is possible the view went away before us, so we only do this if the
  // view is registered.
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);
}

void TabContentsViewWin::Unparent() {
  // Remember who our FocusManager is, we won't be able to access it once
  // unparented.
  focus_manager_ = views::WidgetWin::GetFocusManager();
  // Note that we do not DCHECK on focus_manager_ as it may be NULL when used
  // with an external tab container.
  ::SetParent(GetNativeView(), NULL);
}

void TabContentsViewWin::CreateView(const gfx::Size& initial_size) {
  set_delete_on_destroy(false);
  // Since we create these windows parented to the desktop window initially, we
  // don't want to create them initially visible.
  set_window_style(WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
  WidgetWin::Init(GetDesktopWindow(), gfx::Rect());

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetNativeView());
  drop_target_ = new WebDropTarget(GetNativeView(), tab_contents());
}

RenderWidgetHostView* TabContentsViewWin::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  // If we were showing sad tab, remove it now.
  if (sad_tab_ != NULL) {
    SetContentsView(new views::View());
    sad_tab_ = NULL;
  }

  RenderWidgetHostViewWin* view =
      new RenderWidgetHostViewWin(render_widget_host);
  view->CreateWnd(GetNativeView());
  view->ShowWindow(SW_SHOW);
  return view;
}

gfx::NativeView TabContentsViewWin::GetNativeView() const {
  return WidgetWin::GetNativeView();
}

gfx::NativeView TabContentsViewWin::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow TabContentsViewWin::GetTopLevelNativeWindow() const {
  return ::GetAncestor(GetNativeView(), GA_ROOT);
}

void TabContentsViewWin::GetContainerBounds(gfx::Rect* out) const {
  *out = GetClientAreaScreenBounds();
}

void TabContentsViewWin::StartDragging(const WebDropData& drop_data,
                                       WebDragOperationsMask ops,
                                       const SkBitmap& image,
                                       const gfx::Point& image_offset) {
  drag_handler_ = new TabContentsDragWin(this);
  drag_handler_->StartDragging(drop_data, ops, image, image_offset);
}

void TabContentsViewWin::EndDragging() {
  if (close_tab_after_drag_ends_) {
    close_tab_timer_.Start(base::TimeDelta::FromMilliseconds(0), this,
                           &TabContentsViewWin::CloseTab);
  }

  tab_contents()->SystemDragEnded();
  drag_handler_ = NULL;
}

void TabContentsViewWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(GetNativeView());
    drop_target_ = NULL;
  }

  WidgetWin::OnDestroy();
}

void TabContentsViewWin::SetPageTitle(const std::wstring& title) {
  if (GetNativeView()) {
    // It's possible to get this after the hwnd has been destroyed.
    ::SetWindowText(GetNativeView(), title.c_str());
  }
}

void TabContentsViewWin::OnTabCrashed(base::TerminationStatus /* status */,
                                      int /* error_code */) {
  // Force an invalidation to render sad tab. We will notice we crashed when we
  // paint.
  // Note that it's possible to get this message after the window was destroyed.
  if (::IsWindow(GetNativeView()))
    InvalidateRect(GetNativeView(), NULL, FALSE);
}

void TabContentsViewWin::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See tab_contents_view.h.

  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  if (bounds.size() != size) {
    // Our size really needs to change, invoke SetWindowPos so that we do a
    // layout and all that good stuff. OnWindowPosChanged will invoke WasSized.
    UINT swp_flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(NULL, 0, 0, size.width(), size.height(), swp_flags);
  } else {
    // Our size isn't changing, which means SetWindowPos won't invoke
    // OnWindowPosChanged. We need to invoke WasSized though as the renderer may
    // need to be sized.
    WasSized(bounds.size());
  }
}

void TabContentsViewWin::Focus() {
  if (tab_contents()->interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
    return;
  }

  if (tab_contents()->is_crashed() && sad_tab_ != NULL) {
    sad_tab_->RequestFocus();
    return;
  }

  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (rwhv) {
    ::SetFocus(rwhv->GetNativeView());
    return;
  }

  // Default to focusing our HWND.
  ::SetFocus(GetNativeView());
}

void TabContentsViewWin::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewWin::StoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();

  if (view_storage->RetrieveView(last_focused_view_storage_id_) != NULL)
    view_storage->RemoveView(last_focused_view_storage_id_);

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(GetNativeView());
  if (focus_manager) {
    // |focus_manager| can be NULL if the tab has been detached but still
    // exists.
    views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      view_storage->StoreView(last_focused_view_storage_id_, focused_view);

    // If the focus was on the page, explicitly clear the focus so that we
    // don't end up with the focused HWND not part of the window hierarchy.
    // TODO(brettw) this should move to the view somehow.
    HWND container_hwnd = GetNativeView();
    if (container_hwnd) {
      views::View* focused_view = focus_manager->GetFocusedView();
      if (focused_view) {
        HWND hwnd = focused_view->GetRootView()->GetWidget()->GetNativeView();
        if (container_hwnd == hwnd || ::IsChild(container_hwnd, hwnd))
          focus_manager->ClearFocus();
      }
    }
  }
}

void TabContentsViewWin::RestoreFocus() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);

  if (!last_focused_view) {
    SetInitialFocus();
  } else {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(GetNativeView());

    // If you hit this DCHECK, please report it to Jay (jcampan).
    DCHECK(focus_manager != NULL) << "No focus manager when restoring focus.";

    if (last_focused_view->IsFocusableInRootView() && focus_manager &&
        focus_manager->ContainsView(last_focused_view)) {
      last_focused_view->RequestFocus();
    } else {
      // The focused view may not belong to the same window hierarchy (e.g.
      // if the location bar was focused and the tab is dragged out), or it may
      // no longer be focusable (e.g. if the location bar was focused and then
      // we switched to fullscreen mode).  In that case we default to the
      // default focus.
      SetInitialFocus();
    }
    view_storage->RemoveView(last_focused_view_storage_id_);
  }
}

bool TabContentsViewWin::IsDoingDrag() const {
  return drag_handler_.get() != NULL;
}

void TabContentsViewWin::CancelDragAndCloseTab() {
  DCHECK(IsDoingDrag());
  // We can't close the tab while we're in the drag and
  // |drag_handler_->CancelDrag()| is async.  Instead, set a flag to cancel
  // the drag and when the drag nested message loop ends, close the tab.
  drag_handler_->CancelDrag();
  close_tab_after_drag_ends_ = true;
}

void TabContentsViewWin::GetViewBounds(gfx::Rect* out) const {
  *out = GetWindowScreenBounds();
}

void TabContentsViewWin::UpdateDragCursor(WebDragOperation operation) {
  drop_target_->set_drag_cursor(operation);
}

void TabContentsViewWin::GotFocus() {
  if (tab_contents()->delegate())
    tab_contents()->delegate()->TabContentsFocused(tab_contents());
}

void TabContentsViewWin::TakeFocus(bool reverse) {
  if (!tab_contents()->delegate()->TakeFocus(reverse)) {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(GetNativeView());

    // We may not have a focus manager if the tab has been switched before this
    // message arrived.
    if (focus_manager)
      focus_manager->AdvanceFocus(reverse);
  }
}

views::FocusManager* TabContentsViewWin::GetFocusManager() {
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

void TabContentsViewWin::CloseTab() {
  tab_contents()->Close(tab_contents()->render_view_host());
}

void TabContentsViewWin::ShowContextMenu(const ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (tab_contents()->delegate()->HandleContextMenu(params))
    return;

  context_menu_.reset(new RenderViewContextMenuViews(tab_contents(), params));
  context_menu_->Init();

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetNativeView(), HWND_DESKTOP, &screen_pt, 1);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  context_menu_->RunMenuAt(screen_pt.x, screen_pt.y);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void TabContentsViewWin::ShowPopupMenu(const gfx::Rect& bounds,
                                       int item_height,
                                       double item_font_size,
                                       int selected_item,
                                       const std::vector<WebMenuItem>& items,
                                       bool right_aligned) {
  // External popup menus are only used on Mac.
  NOTREACHED();
}

void TabContentsViewWin::OnHScroll(int scroll_type, short position,
                                   HWND scrollbar) {
  ScrollCommon(WM_HSCROLL, scroll_type, position, scrollbar);
}

void TabContentsViewWin::OnMouseLeave() {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (tab_contents()->delegate())
    tab_contents()->delegate()->ContentsMouseEvent(
        tab_contents(), views::Screen::GetCursorScreenPoint(), false);
  SetMsgHandled(FALSE);
}

LRESULT TabContentsViewWin::OnMouseRange(UINT msg,
                                         WPARAM w_param, LPARAM l_param) {
  if (tab_contents()->is_crashed() && sad_tab_ != NULL) {
    return WidgetWin::OnMouseRange(msg, w_param, l_param);
  }

  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      // Make sure this TabContents is activated when it is clicked on.
      if (tab_contents()->delegate())
        tab_contents()->delegate()->ActivateContents(tab_contents());
      break;
    }
    case WM_MOUSEMOVE:
      // Let our delegate know that the mouse moved (useful for resetting status
      // bubble state).
      if (tab_contents()->delegate())
        tab_contents()->delegate()->ContentsMouseEvent(
            tab_contents(), views::Screen::GetCursorScreenPoint(), true);
      break;
    default:
      break;
  }

  return 0;
}

void TabContentsViewWin::OnPaint(HDC junk_dc) {
  if (tab_contents()->render_view_host() &&
      !tab_contents()->render_view_host()->IsRenderViewLive()) {
    base::TerminationStatus status =
        tab_contents()->render_view_host()->render_view_termination_status();
    SadTabView::Kind kind =
        status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
        SadTabView::KILLED : SadTabView::CRASHED;
    sad_tab_ = new SadTabView(tab_contents(), kind);
    SetContentsView(sad_tab_);
    CRect cr;
    GetClientRect(&cr);
    sad_tab_->SetBoundsRect(gfx::Rect(cr));
    gfx::CanvasSkiaPaint canvas(GetNativeView(), true);
    sad_tab_->Paint(&canvas);
    return;
  }

  // We need to do this to validate the dirty area so we don't end up in a
  // WM_PAINTstorm that causes other mysterious bugs (such as WM_TIMERs not
  // firing etc). It doesn't matter that we don't have any non-clipped area.
  CPaintDC dc(GetNativeView());
  SetMsgHandled(FALSE);
}

// A message is reflected here from view().
// Return non-zero to indicate that it is handled here.
// Return 0 to allow view() to further process it.
LRESULT TabContentsViewWin::OnReflectedMessage(UINT msg, WPARAM w_param,
                                        LPARAM l_param) {
  MSG* message = reinterpret_cast<MSG*>(l_param);
  switch (message->message) {
    case WM_MOUSEWHEEL:
      // This message is reflected from the view() to this window.
      if (GET_KEYSTATE_WPARAM(message->wParam) & MK_CONTROL) {
        WheelZoom(GET_WHEEL_DELTA_WPARAM(message->wParam));
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

void TabContentsViewWin::OnVScroll(int scroll_type, short position,
                                   HWND scrollbar) {
  ScrollCommon(WM_VSCROLL, scroll_type, position, scrollbar);
}

void TabContentsViewWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (window_pos->flags & SWP_HIDEWINDOW) {
    WasHidden();
  } else {
    // The TabContents was shown by a means other than the user selecting a
    // Tab, e.g. the window was minimized then restored.
    if (window_pos->flags & SWP_SHOWWINDOW)
      WasShown();

    // Unless we were specifically told not to size, cause the renderer to be
    // sized to the new bounds, which forces a repaint. Not required for the
    // simple minimize-restore case described above, for example, since the
    // size hasn't changed.
    if (!(window_pos->flags & SWP_NOSIZE))
      WasSized(gfx::Size(window_pos->cx, window_pos->cy));
  }
}

void TabContentsViewWin::OnSize(UINT param, const CSize& size) {
  // NOTE: Because TabContentsViewWin handles OnWindowPosChanged without calling
  // DefWindowProc, OnSize is NOT called on window resize. This handler
  // is called only once when the window is created.

  // Don't call base class OnSize to avoid useless layout for 0x0 size.
  // We will get OnWindowPosChanged later and layout root view in WasSized.

  // Hack for thinkpad touchpad driver.
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

LRESULT TabContentsViewWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  // Hack for thinkpad mouse wheel driver. We have set the fake scroll bars
  // to receive scroll messages from thinkpad touchpad driver. Suppress
  // painting of scrollbars by returning 0 size for them.
  return 0;
}

void TabContentsViewWin::OnNCPaint(HRGN rgn) {
  // Suppress default WM_NCPAINT handling. We don't need to do anything
  // here since the view will draw everything correctly.
}

void TabContentsViewWin::ScrollCommon(UINT message, int scroll_type,
                                      short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // Trackpad scroll wheel emulation.
  if (!ScrollZoom(scroll_type)) {
    // Reflect scroll message to the view() to give it a chance
    // to process scrolling.
    SendMessage(GetContentNativeView(), message,
                MAKELONG(scroll_type, position),
                reinterpret_cast<LPARAM>(scrollbar));
  }
}

void TabContentsViewWin::WasHidden() {
  tab_contents()->HideContents();
}

void TabContentsViewWin::WasShown() {
  tab_contents()->ShowContents();
}

void TabContentsViewWin::WasSized(const gfx::Size& size) {
  if (tab_contents()->interstitial_page())
    tab_contents()->interstitial_page()->SetSize(size);
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);

  // We have to layout root view here because we handle OnWindowPosChanged
  // without calling DefWindowProc (it sends OnSize and OnMove) so we don't
  // receive OnSize. For performance reasons call SetBounds instead of
  // LayoutRootView, we actually don't need paint event and we know new size.
  GetRootView()->SetBounds(0, 0, size.width(), size.height());
}

bool TabContentsViewWin::ScrollZoom(int scroll_type) {
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

    WheelZoom(distance);
    return true;
  }
  return false;
}

void TabContentsViewWin::WheelZoom(int distance) {
  if (tab_contents()->delegate()) {
    bool zoom_in = distance > 0;
    tab_contents()->delegate()->ContentsZoomChange(zoom_in);
  }
}
