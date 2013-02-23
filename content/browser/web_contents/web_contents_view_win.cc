// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_win.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/web_contents_drag_win.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_drag_dest_win.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/base/win/hidden_window.h"
#include "ui/base/win/hwnd_subclass.h"
#include "ui/gfx/screen.h"

namespace content {
WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewWin* rv = new WebContentsViewWin(web_contents, delegate);
  *render_view_host_delegate_view = rv;
  return rv;
}

namespace {

typedef std::map<HWND, WebContentsViewWin*> HwndToWcvMap;
HwndToWcvMap hwnd_to_wcv_map;

void RemoveHwndToWcvMapEntry(WebContentsViewWin* wcv) {
  HwndToWcvMap::iterator it;
  for (it = hwnd_to_wcv_map.begin(); it != hwnd_to_wcv_map.end();) {
    if (it->second == wcv)
      hwnd_to_wcv_map.erase(it++);
    else
      ++it;
  }
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
  HwndToWcvMap::iterator it = hwnd_to_wcv_map.find(hwnd);
  if (it == hwnd_to_wcv_map.end())
    return TRUE;  // must return TRUE to continue enumeration.
  WebContentsViewWin* wcv = it->second;
  RenderWidgetHostViewWin* rwhv = static_cast<RenderWidgetHostViewWin*>(
      wcv->web_contents()->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->UpdateScreenInfo(rwhv->GetNativeView());

  return TRUE;  // must return TRUE to continue enumeration.
}

class PositionChangedMessageFilter : public ui::HWNDMessageFilter {
 public:
  PositionChangedMessageFilter() {}

 private:
  // Overridden from ui::HWNDMessageFilter:
  virtual bool FilterMessage(HWND hwnd,
                             UINT message,
                             WPARAM w_param,
                             LPARAM l_param,
                             LRESULT* l_result) OVERRIDE {
    if (message == WM_WINDOWPOSCHANGED || message == WM_SETTINGCHANGE)
      EnumChildWindows(hwnd, EnumChildProc, 0);

    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(PositionChangedMessageFilter);
};

void AddFilterToParentHwndSubclass(HWND hwnd, ui::HWNDMessageFilter* filter) {
  HWND parent = ::GetAncestor(hwnd, GA_ROOT);
  if (parent) {
    ui::HWNDSubclass::RemoveFilterFromAllTargets(filter);
    ui::HWNDSubclass::AddFilterToTarget(parent, filter);
  }
}

}  // namespace namespace

WebContentsViewWin::WebContentsViewWin(WebContentsImpl* web_contents,
                                       WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      hwnd_message_filter_(new PositionChangedMessageFilter) {
}

WebContentsViewWin::~WebContentsViewWin() {
  RemoveHwndToWcvMapEntry(this);

  if (IsWindow(hwnd()))
    DestroyWindow(hwnd());
}

void WebContentsViewWin::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context) {
  initial_size_ = initial_size;

  set_window_style(WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

  Init(ui::GetHiddenWindow(), gfx::Rect(initial_size_));

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetNativeView());
  drag_dest_ = new WebDragDest(hwnd(), web_contents_);
  if (delegate_.get()) {
    WebDragDestDelegate* delegate = delegate_->GetDragDestDelegate();
    if (delegate)
      drag_dest_->set_delegate(delegate);
  }
}

RenderWidgetHostView* WebContentsViewWin::CreateViewForWidget(
    RenderWidgetHost* render_widget_host)  {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  RenderWidgetHostViewWin* view = static_cast<RenderWidgetHostViewWin*>(
      RenderWidgetHostView::CreateViewForWidget(render_widget_host));
  view->CreateWnd(GetNativeView());
  view->ShowWindow(SW_SHOW);
  view->SetSize(initial_size_);
  return view;
}

RenderWidgetHostView* WebContentsViewWin::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

gfx::NativeView WebContentsViewWin::GetNativeView() const {
  return hwnd();
}

gfx::NativeView WebContentsViewWin::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : NULL;
}

gfx::NativeWindow WebContentsViewWin::GetTopLevelNativeWindow() const {
  return GetParent(GetNativeView());
}

void WebContentsViewWin::GetContainerBounds(gfx::Rect *out) const {
  // Copied from NativeWidgetWin::GetClientAreaScreenBounds().
  RECT r;
  GetClientRect(hwnd(), &r);
  POINT point = { r.left, r.top };
  ClientToScreen(hwnd(), &point);
  *out = gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

void WebContentsViewWin::SetPageTitle(const string16& title) {
  // It's possible to get this after the hwnd has been destroyed.
  if (GetNativeView())
    ::SetWindowText(GetNativeView(), title.c_str());
}

void WebContentsViewWin::OnTabCrashed(base::TerminationStatus status,
                                      int error_code) {
}

void WebContentsViewWin::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  if (bounds.size() != size) {
    SetWindowPos(hwnd(), NULL, 0, 0, size.width(), size.height(),
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    if (web_contents_->GetInterstitialPage())
      web_contents_->GetInterstitialPage()->SetSize(size);
    RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
    if (rwhv)
      rwhv->SetSize(size);
  }
}

void WebContentsViewWin::RenderViewCreated(RenderViewHost* host) {
}

void WebContentsViewWin::RenderViewSwappedIn(RenderViewHost* host) {
}

void WebContentsViewWin::Focus() {
  if (web_contents_->GetInterstitialPage()) {
    web_contents_->GetInterstitialPage()->Focus();
    return;
  }

  if (delegate_.get() && delegate_->Focus())
    return;

  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->Focus();
}

void WebContentsViewWin::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void WebContentsViewWin::StoreFocus() {
  if (delegate_.get())
    delegate_->StoreFocus();
}

void WebContentsViewWin::RestoreFocus() {
  if (delegate_.get())
    delegate_->RestoreFocus();
}

WebDropData* WebContentsViewWin::GetDropData() const {
  return drag_dest_->current_drop_data();
}

bool WebContentsViewWin::IsEventTracking() const {
  return false;
}

void WebContentsViewWin::CloseTabAfterEventTracking() {
}

gfx::Rect WebContentsViewWin::GetViewBounds() const {
  RECT r;
  GetWindowRect(hwnd(), &r);
  return gfx::Rect(r);
}

void WebContentsViewWin::ShowContextMenu(
    const ContextMenuParams& params,
    ContextMenuSourceType type) {
  if (delegate_.get())
    delegate_->ShowContextMenu(params, type);
}

void WebContentsViewWin::ShowPopupMenu(const gfx::Rect& bounds,
                                       int item_height,
                                       double item_font_size,
                                       int selected_item,
                                       const std::vector<WebMenuItem>& items,
                                       bool right_aligned,
                                       bool allow_multiple_selection) {
  // External popup menus are only used on Mac and Android.
  NOTIMPLEMENTED();
}

void WebContentsViewWin::StartDragging(const WebDropData& drop_data,
                                       WebKit::WebDragOperationsMask operations,
                                       const gfx::ImageSkia& image,
                                       const gfx::Vector2d& image_offset,
                                       const DragEventSourceInfo& event_info) {
  drag_handler_ = new WebContentsDragWin(
      GetNativeView(),
      web_contents_,
      drag_dest_,
      base::Bind(&WebContentsViewWin::EndDragging, base::Unretained(this)));
  drag_handler_->StartDragging(drop_data, operations, image, image_offset);
}

void WebContentsViewWin::UpdateDragCursor(WebKit::WebDragOperation operation) {
  drag_dest_->set_drag_cursor(operation);
}

void WebContentsViewWin::GotFocus() {
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->WebContentsFocused(web_contents_);
}

void WebContentsViewWin::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      !web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse) &&
      delegate_.get()) {
    delegate_->TakeFocus(reverse);
  }
}

void WebContentsViewWin::EndDragging() {
  drag_handler_ = NULL;
  web_contents_->SystemDragEnded();
}

void WebContentsViewWin::CloseTab() {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->GetDelegate()->Close(rvh);
}

LRESULT WebContentsViewWin::OnCreate(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  hwnd_to_wcv_map.insert(std::make_pair(hwnd(), this));
  AddFilterToParentHwndSubclass(hwnd(), hwnd_message_filter_.get());
  return 0;
}

LRESULT WebContentsViewWin::OnDestroy(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  if (drag_dest_.get()) {
    RevokeDragDrop(GetNativeView());
    drag_dest_ = NULL;
  }
  if (drag_handler_.get()) {
    drag_handler_->CancelDrag();
    drag_handler_ = NULL;
  }
  return 0;
}

LRESULT WebContentsViewWin::OnWindowPosChanged(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {

  // Our parent might have changed. So we re-install our hwnd message filter.
  AddFilterToParentHwndSubclass(hwnd(), hwnd_message_filter_.get());

  WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(lparam);
  if (window_pos->flags & SWP_HIDEWINDOW) {
    web_contents_->WasHidden();
    return 0;
  }

  // The WebContents was shown by a means other than the user selecting a
  // Tab, e.g. the window was minimized then restored.
  if (window_pos->flags & SWP_SHOWWINDOW)
    web_contents_->WasShown();

  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    RenderWidgetHostViewWin* view = static_cast<RenderWidgetHostViewWin*>(rwhv);
    view->UpdateScreenInfo(view->GetNativeView());
  }

  // Unless we were specifically told not to size, cause the renderer to be
  // sized to the new bounds, which forces a repaint. Not required for the
  // simple minimize-restore case described above, for example, since the
  // size hasn't changed.
  if (window_pos->flags & SWP_NOSIZE)
    return 0;

  gfx::Size size(window_pos->cx, window_pos->cy);
  if (web_contents_->GetInterstitialPage())
    web_contents_->GetInterstitialPage()->SetSize(size);
  if (rwhv)
    rwhv->SetSize(size);

  if (delegate_.get())
    delegate_->SizeChanged(size);

  return 0;
}

LRESULT WebContentsViewWin::OnMouseDown(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // Make sure this WebContents is activated when it is clicked on.
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->ActivateContents(web_contents_);
  return 0;
}

LRESULT WebContentsViewWin::OnMouseMove(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (web_contents_->GetDelegate()) {
    web_contents_->GetDelegate()->ContentsMouseEvent(
        web_contents_,
        gfx::Screen::GetNativeScreen()->GetCursorScreenPoint(),
        true);
  }
  return 0;
}

LRESULT WebContentsViewWin::OnNCCalcSize(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // Hack for ThinkPad mouse wheel driver. We have set the fake scroll bars
  // to receive scroll messages from ThinkPad touch-pad driver. Suppress
  // painting of scrollbars by returning 0 size for them.
  return 0;
}

LRESULT WebContentsViewWin::OnNCHitTest(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  return HTTRANSPARENT;
}

LRESULT WebContentsViewWin::OnScroll(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  int scroll_type = LOWORD(wparam);
  short position = HIWORD(wparam);
  HWND scrollbar = reinterpret_cast<HWND>(lparam);
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

    web_contents_->GetDelegate()->ContentsZoomChange(distance > 0);
    return 0;
  }

  // Reflect scroll message to the view() to give it a chance
  // to process scrolling.
  SendMessage(GetContentNativeView(), message, wparam, lparam);
  return 0;
}

LRESULT WebContentsViewWin::OnSize(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  // NOTE: Because we handle OnWindowPosChanged without calling DefWindowProc,
  // OnSize is NOT called on window resize. This handler is called only once
  // when the window is created.
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

  ::SetScrollInfo(hwnd(), SB_HORZ, &si, FALSE);
  ::SetScrollInfo(hwnd(), SB_VERT, &si, FALSE);

  return 1;
}

}  // namespace content
