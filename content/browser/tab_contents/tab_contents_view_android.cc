// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_view_android.h"

#include "base/logging.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"

TabContentsViewAndroid::TabContentsViewAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

TabContentsViewAndroid::~TabContentsViewAndroid() {
}

void TabContentsViewAndroid::CreateView(const gfx::Size& initial_size) {
  NOTIMPLEMENTED();
}

RenderWidgetHostView* TabContentsViewAndroid::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView TabContentsViewAndroid::GetNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView TabContentsViewAndroid::GetContentNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeWindow TabContentsViewAndroid::GetTopLevelNativeWindow() const {
  NOTIMPLEMENTED();
  return NULL;
}

void TabContentsViewAndroid::GetContainerBounds(gfx::Rect* out) const {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::SetPageTitle(const string16& title) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::OnTabCrashed(base::TerminationStatus status,
                                          int error_code) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::SizeContents(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::RenderViewCreated(RenderViewHost* host) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::Focus() {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::SetInitialFocus() {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::StoreFocus() {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::RestoreFocus() {
  NOTIMPLEMENTED();
}

bool TabContentsViewAndroid::IsDoingDrag() const {
  NOTIMPLEMENTED();
  return false;
}

void TabContentsViewAndroid::CancelDragAndCloseTab() {
  NOTIMPLEMENTED();
}

bool TabContentsViewAndroid::IsEventTracking() const {
  NOTIMPLEMENTED();
  return false;
}

void TabContentsViewAndroid::CloseTabAfterEventTracking() {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::GetViewBounds(gfx::Rect* out) const {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::InstallOverlayView(gfx::NativeView view) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::RemoveOverlayView() {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::ConfirmTouchEvent(bool handled) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::DidSetNeedTouchEvents(bool need_touch_events) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::CreateNewWidget(
    int route_id, WebKit::WebPopupType popup_type) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::CreateNewFullscreenWidget(int route_id) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::ShowCreatedWindow(int route_id,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::ShowCreatedWidget(
    int route_id, const gfx::Rect& initial_pos) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::ShowCreatedFullscreenWidget(int route_id) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::ShowContextMenu(const ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask allowed_ops,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::UpdateDragCursor(WebKit::WebDragOperation op) {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::GotFocus() {
  NOTIMPLEMENTED();
}

void TabContentsViewAndroid::TakeFocus(bool reverse) {
  NOTIMPLEMENTED();
}
