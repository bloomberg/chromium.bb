// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_view_win.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::WebContents;

namespace {

// We need to have a parent window for the compositing code to work correctly.
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
class TempParent : public ui::WindowImpl {
 public:
  static TempParent* Get() {
    static TempParent* g_temp_parent;
    if (!g_temp_parent) {
      g_temp_parent = new TempParent();

      g_temp_parent->set_window_style(WS_POPUP);
      g_temp_parent->set_window_ex_style(WS_EX_TOOLWINDOW);
      g_temp_parent->Init(GetDesktopWindow(), gfx::Rect());
      EnableWindow(g_temp_parent->hwnd(), FALSE);
    }
    return g_temp_parent;
  }

 private:
  BEGIN_MSG_MAP_EX(TabContentsViewWin)
  END_MSG_MAP()
};

}  // namespace namespace

TabContentsViewWin::TabContentsViewWin(WebContents* web_contents)
    : parent_(NULL),
      tab_contents_(static_cast<TabContents*>(web_contents)),
      view_(NULL) {
}

TabContentsViewWin::~TabContentsViewWin() {
}

void TabContentsViewWin::SetParent(HWND parent) {
  DCHECK(!parent_);
  parent_ = parent;

  ::SetParent(hwnd(), parent_);
}

void TabContentsViewWin::CreateView(const gfx::Size& initial_size) {
  initial_size_ = initial_size;

  set_window_style(WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

  Init(TempParent::Get()->hwnd(), gfx::Rect(initial_size_));
}

RenderWidgetHostView* TabContentsViewWin::CreateViewForWidget(
    RenderWidgetHost* render_widget_host)  {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  view_ = new RenderWidgetHostViewWin(render_widget_host);
  view_->CreateWnd(GetNativeView());
  view_->ShowWindow(SW_SHOW);
  view_->SetSize(initial_size_);
  return view_;
}

gfx::NativeView TabContentsViewWin::GetNativeView() const {
  return hwnd();
}

gfx::NativeView TabContentsViewWin::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : NULL;
}

gfx::NativeWindow TabContentsViewWin::GetTopLevelNativeWindow() const {
  return parent_;
}

void TabContentsViewWin::GetContainerBounds(gfx::Rect *out) const {
  // Copied from NativeWidgetWin::GetClientAreaScreenBounds().
  RECT r;
  GetClientRect(hwnd(), &r);
  POINT point = { r.left, r.top };
  ClientToScreen(hwnd(), &point);
  *out = gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

void TabContentsViewWin::SetPageTitle(const string16& title) {
}

void TabContentsViewWin::OnTabCrashed(base::TerminationStatus status,
                                      int error_code) {
  // TODO(avi): No other TCV implementation does anything in this callback. Can
  // this be moved elsewhere so that |OnTabCrashed| can be removed everywhere?
  view_ = NULL;
}

void TabContentsViewWin::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds;
  GetContainerBounds(&bounds);
  if (bounds.size() != size) {
    SetWindowPos(hwnd(), NULL, 0, 0, size.width(), size.height(),
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    if (tab_contents_->GetInterstitialPage())
      tab_contents_->GetInterstitialPage()->SetSize(size);
    RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
    if (rwhv)
      rwhv->SetSize(size);
  }
}

void TabContentsViewWin::RenderViewCreated(RenderViewHost* host) {
}

void TabContentsViewWin::Focus() {
  if (tab_contents_->GetInterstitialPage()) {
    tab_contents_->GetInterstitialPage()->Focus();
    return;
  }

  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->Focus();
  } else {
    SetFocus(hwnd());
  }
}

void TabContentsViewWin::SetInitialFocus() {
  if (tab_contents_->FocusLocationBarByDefault())
    tab_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewWin::StoreFocus() {
  // TODO(jam): why is this on TabContentsView?
  NOTREACHED();
}

void TabContentsViewWin::RestoreFocus() {
  NOTREACHED();
}

bool TabContentsViewWin::IsDoingDrag() const {
  return false;
}

void TabContentsViewWin::CancelDragAndCloseTab() {
}

bool TabContentsViewWin::IsEventTracking() const {
  return false;
}

void TabContentsViewWin::CloseTabAfterEventTracking() {
}

void TabContentsViewWin::GetViewBounds(gfx::Rect* out) const {
  RECT r;
  GetWindowRect(hwnd(), &r);
  *out = gfx::Rect(r);
}

void TabContentsViewWin::InstallOverlayView(gfx::NativeView view) {
  NOTREACHED();
}

void TabContentsViewWin::RemoveOverlayView() {
  NOTREACHED();
}

void TabContentsViewWin::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  tab_contents_view_helper_.CreateNewWindow(tab_contents_, route_id, params);
}

void TabContentsViewWin::CreateNewWidget(int route_id,
                                         WebKit::WebPopupType popup_type) {
  tab_contents_view_helper_.CreateNewWidget(tab_contents_,
                                            route_id,
                                            false,
                                            popup_type);
}

void TabContentsViewWin::CreateNewFullscreenWidget(int route_id) {
  tab_contents_view_helper_.CreateNewWidget(tab_contents_,
                                            route_id,
                                            true,
                                            WebKit::WebPopupTypeNone);
}

void TabContentsViewWin::ShowCreatedWindow(int route_id,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture) {
  tab_contents_view_helper_.ShowCreatedWindow(
      tab_contents_, route_id, disposition, initial_pos, user_gesture);
}

void TabContentsViewWin::ShowCreatedWidget(int route_id,
                                           const gfx::Rect& initial_pos) {
  tab_contents_view_helper_.ShowCreatedWidget(tab_contents_,
                                              route_id,
                                              false,
                                              initial_pos);
}

void TabContentsViewWin::ShowCreatedFullscreenWidget(int route_id) {
  tab_contents_view_helper_.ShowCreatedWidget(tab_contents_,
                                              route_id,
                                              true,
                                              gfx::Rect());
}

void TabContentsViewWin::ShowContextMenu(const ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::ShowPopupMenu(const gfx::Rect& bounds,
                                       int item_height,
                                       double item_font_size,
                                       int selected_item,
                                       const std::vector<WebMenuItem>& items,
                                       bool right_aligned) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::StartDragging(const WebDropData& drop_data,
                                       WebKit::WebDragOperationsMask operations,
                                       const SkBitmap& image,
                                       const gfx::Point& image_offset) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::UpdateDragCursor(WebKit::WebDragOperation operation) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::GotFocus() {
  if (tab_contents_->GetDelegate())
    tab_contents_->GetDelegate()->WebContentsFocused(tab_contents_);
}

void TabContentsViewWin::TakeFocus(bool reverse) {
  if (tab_contents_->GetDelegate())
    tab_contents_->GetDelegate()->TakeFocus(reverse);
}

LRESULT TabContentsViewWin::OnWindowPosChanged(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(lparam);
  if (window_pos->flags & SWP_NOSIZE)
    return 0;

  gfx::Size size(window_pos->cx, window_pos->cy);
  if (tab_contents_->GetInterstitialPage())
    tab_contents_->GetInterstitialPage()->SetSize(size);
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);

  return 0;
}
