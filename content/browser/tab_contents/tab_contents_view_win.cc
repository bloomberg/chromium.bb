// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_view_win.h"

#include "content/browser/renderer_host/render_widget_host_view_win.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

TabContentsViewWin::TabContentsViewWin(TabContents* tab_contents)
    : parent_(NULL),
      tab_contents_(tab_contents),
      view_(NULL) {
}

TabContentsViewWin::~TabContentsViewWin() {
}

void TabContentsViewWin::SetParent(HWND parent) {
  DCHECK(!parent_);
  parent_ = parent;

  set_window_style(WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

  Init(parent_, gfx::Rect(initial_size_));
}

void TabContentsViewWin::CreateView(const gfx::Size& initial_size) {
  initial_size_ = initial_size;
}

RenderWidgetHostView* TabContentsViewWin::CreateViewForWidget(
    RenderWidgetHost* render_widget_host)  {
  if (view_)
    delete view_;  // TODO(jam): need to do anything else?

  view_ = new RenderWidgetHostViewWin(render_widget_host);
  view_->CreateWnd(GetNativeView());
  view_->ShowWindow(SW_SHOW);
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

void TabContentsViewWin::SetPageTitle(const std::wstring& title) {
}

void TabContentsViewWin::OnTabCrashed(base::TerminationStatus status,
                                      int error_code) {
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
    if (tab_contents_->interstitial_page())
      tab_contents_->interstitial_page()->SetSize(size);
    RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
    if (rwhv)
      rwhv->SetSize(size);
  }
}

void TabContentsViewWin::RenderViewCreated(RenderViewHost* host) {
}

void TabContentsViewWin::Focus() {
  if (tab_contents_->interstitial_page()) {
    tab_contents_->interstitial_page()->Focus();
    return;
  }

  if (tab_contents_->constrained_window_count() > 0) {
    ConstrainedWindow* window = *tab_contents_->constrained_window_begin();
    DCHECK(window);
    window->FocusConstrainedWindow();
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

void TabContentsViewWin::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  NOTIMPLEMENTED();
}


void TabContentsViewWin::CreateNewWidget(int route_id,
                                         WebKit::WebPopupType popup_type) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::CreateNewFullscreenWidget(int route_id) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::ShowCreatedWindow(int route_id,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::ShowCreatedWidget(int route_id,
                                           const gfx::Rect& initial_pos) {
  NOTIMPLEMENTED();
}

void TabContentsViewWin::ShowCreatedFullscreenWidget(int route_id) {
  NOTIMPLEMENTED();
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
  if (tab_contents_->delegate())
    tab_contents_->delegate()->TabContentsFocused(tab_contents_);
}

void TabContentsViewWin::TakeFocus(bool reverse) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->TakeFocus(reverse);
}

LRESULT TabContentsViewWin::OnWindowPosChanged(
    UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(lparam);
  if (window_pos->flags & SWP_NOSIZE)
    return 0;

  gfx::Size size(window_pos->cx, window_pos->cy);
  if (tab_contents_->interstitial_page())
    tab_contents_->interstitial_page()->SetSize(size);
  RenderWidgetHostView* rwhv = tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);

  return 0;
}
