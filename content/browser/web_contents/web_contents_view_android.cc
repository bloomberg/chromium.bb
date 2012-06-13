// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include "base/logging.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewAndroid* rv = new WebContentsViewAndroid(web_contents);
  *render_view_host_delegate_view = rv;
  return rv;
}
}

WebContentsViewAndroid::WebContentsViewAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

WebContentsViewAndroid::~WebContentsViewAndroid() {
}

void WebContentsViewAndroid::CreateView(const gfx::Size& initial_size) {
  NOTIMPLEMENTED();
}

content::RenderWidgetHostView* WebContentsViewAndroid::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host) {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView WebContentsViewAndroid::GetNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView WebContentsViewAndroid::GetContentNativeView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeWindow WebContentsViewAndroid::GetTopLevelNativeWindow() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewAndroid::GetContainerBounds(gfx::Rect* out) const {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::SetPageTitle(const string16& title) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::OnTabCrashed(base::TerminationStatus status,
                                          int error_code) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::SizeContents(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::RenderViewCreated(content::RenderViewHost* host) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::Focus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::SetInitialFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::StoreFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::RestoreFocus() {
  NOTIMPLEMENTED();
}

bool WebContentsViewAndroid::IsDoingDrag() const {
  NOTIMPLEMENTED();
  return false;
}

void WebContentsViewAndroid::CancelDragAndCloseTab() {
  NOTIMPLEMENTED();
}

WebDropData* WebContentsViewAndroid::GetDropData() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool WebContentsViewAndroid::IsEventTracking() const {
  NOTIMPLEMENTED();
  return false;
}

void WebContentsViewAndroid::CloseTabAfterEventTracking() {
  NOTIMPLEMENTED();
}

gfx::Rect WebContentsViewAndroid::GetViewBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void WebContentsViewAndroid::ShowContextMenu(
    const content::ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask allowed_ops,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::UpdateDragCursor(WebKit::WebDragOperation op) {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::GotFocus() {
  NOTIMPLEMENTED();
}

void WebContentsViewAndroid::TakeFocus(bool reverse) {
  NOTIMPLEMENTED();
}
