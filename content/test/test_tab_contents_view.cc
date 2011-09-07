// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_tab_contents_view.h"

TestTabContentsView::TestTabContentsView() {
}

TestTabContentsView::~TestTabContentsView() {
}

void TestTabContentsView::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
}

void TestTabContentsView::CreateNewWidget(int route_id,
                                          WebKit::WebPopupType popup_type) {
}

void TestTabContentsView::CreateNewFullscreenWidget(int route_id) {
}

void TestTabContentsView::ShowCreatedWindow(int route_id,
                                            WindowOpenDisposition disposition,
                                            const gfx::Rect& initial_pos,
                                            bool user_gesture) {
}

void TestTabContentsView::ShowCreatedWidget(int route_id,
                                            const gfx::Rect& initial_pos) {
}

void TestTabContentsView::ShowCreatedFullscreenWidget(int route_id) {
}

void TestTabContentsView::ShowContextMenu(const ContextMenuParams& params) {
}

void TestTabContentsView::ShowPopupMenu(const gfx::Rect& bounds,
                                        int item_height,
                                        double item_font_size,
                                        int selected_item,
                                        const std::vector<WebMenuItem>& items,
                                        bool right_aligned) {
}

void TestTabContentsView::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask allowed_ops,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
}

void TestTabContentsView::UpdateDragCursor(WebKit::WebDragOperation operation) {
}

void TestTabContentsView::GotFocus() {
}

void TestTabContentsView::TakeFocus(bool reverse) {
}

void TestTabContentsView::CreateView(const gfx::Size& initial_size) {
}

RenderWidgetHostView* TestTabContentsView::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  return NULL;
}

gfx::NativeView TestTabContentsView::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeView TestTabContentsView::GetContentNativeView() const {
  return gfx::NativeView();
}

gfx::NativeWindow TestTabContentsView::GetTopLevelNativeWindow() const {
  return gfx::NativeWindow();
}

void TestTabContentsView::GetContainerBounds(gfx::Rect *out) const {
}

void TestTabContentsView::SetPageTitle(const std::wstring& title) {
}

void TestTabContentsView::OnTabCrashed(base::TerminationStatus status,
                                       int error_code) {
}

void TestTabContentsView::SizeContents(const gfx::Size& size) {
}

void TestTabContentsView::RenderViewCreated(RenderViewHost* host) {
}

void TestTabContentsView::Focus() {
}

void TestTabContentsView::SetInitialFocus() {
}

void TestTabContentsView::StoreFocus() {
}

void TestTabContentsView::RestoreFocus() {
}

bool TestTabContentsView::IsDoingDrag() const {
  return false;
}

void TestTabContentsView::CancelDragAndCloseTab() {
}

bool TestTabContentsView::IsEventTracking() const {
  return false;
}

void TestTabContentsView::CloseTabAfterEventTracking() {
}

void TestTabContentsView::GetViewBounds(gfx::Rect* out) const {
}
