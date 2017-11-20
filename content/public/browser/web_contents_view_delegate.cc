// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_view_delegate.h"

#include <stddef.h>

namespace content {

WebContentsViewDelegate::~WebContentsViewDelegate() {
}

gfx::NativeWindow WebContentsViewDelegate::GetNativeWindow() {
  return nullptr;
}

WebDragDestDelegate* WebContentsViewDelegate::GetDragDestDelegate() {
  return nullptr;
}

void WebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
}

void WebContentsViewDelegate::StoreFocus() {
}

bool WebContentsViewDelegate::RestoreFocus() {
  return false;
}

void WebContentsViewDelegate::ResetStoredFocus() {}

bool WebContentsViewDelegate::Focus() {
  return false;
}

bool WebContentsViewDelegate::TakeFocus(bool reverse) {
  return false;
}

void WebContentsViewDelegate::SizeChanged(const gfx::Size& size) {
}

void WebContentsViewDelegate::OverrideDisplayColorSpace(
    gfx::ColorSpace* color_space) {}

void* WebContentsViewDelegate::CreateRenderWidgetHostViewDelegate(
    RenderWidgetHost* render_widget_host,
    bool is_popup) {
  return nullptr;
}

}  // namespace content
