// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_web_contents_view.h"

namespace content {

TestWebContentsView::TestWebContentsView() {
}

TestWebContentsView::~TestWebContentsView() {
}

void TestWebContentsView::ShowContextMenu(
    const ContextMenuParams& params,
    const content::ContextMenuSourceType& type) {
}

void TestWebContentsView::ShowPopupMenu(const gfx::Rect& bounds,
                                        int item_height,
                                        double item_font_size,
                                        int selected_item,
                                        const std::vector<WebMenuItem>& items,
                                        bool right_aligned,
                                        bool allow_multiple_selection) {
}

void TestWebContentsView::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Point& image_offset) {
}

void TestWebContentsView::UpdateDragCursor(WebKit::WebDragOperation operation) {
}

void TestWebContentsView::GotFocus() {
}

void TestWebContentsView::TakeFocus(bool reverse) {
}

void TestWebContentsView::CreateView(const gfx::Size& initial_size) {
}

RenderWidgetHostView* TestWebContentsView::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  return NULL;
}

gfx::NativeView TestWebContentsView::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeView TestWebContentsView::GetContentNativeView() const {
  return gfx::NativeView();
}

gfx::NativeWindow TestWebContentsView::GetTopLevelNativeWindow() const {
  return gfx::NativeWindow();
}

void TestWebContentsView::GetContainerBounds(gfx::Rect *out) const {
}

void TestWebContentsView::SetPageTitle(const string16& title) {
}

void TestWebContentsView::OnTabCrashed(base::TerminationStatus status,
                                       int error_code) {
}

void TestWebContentsView::SizeContents(const gfx::Size& size) {
}

void TestWebContentsView::RenderViewCreated(RenderViewHost* host) {
}

void TestWebContentsView::Focus() {
}

void TestWebContentsView::SetInitialFocus() {
}

void TestWebContentsView::StoreFocus() {
}

void TestWebContentsView::RestoreFocus() {
}

bool TestWebContentsView::IsDoingDrag() const {
  return false;
}

void TestWebContentsView::CancelDragAndCloseTab() {
}

WebDropData* TestWebContentsView::GetDropData() const {
  return NULL;
}

bool TestWebContentsView::IsEventTracking() const {
  return false;
}

void TestWebContentsView::CloseTabAfterEventTracking() {
}

gfx::Rect TestWebContentsView::GetViewBounds() const {
  return gfx::Rect();
}

}  // namespace content
