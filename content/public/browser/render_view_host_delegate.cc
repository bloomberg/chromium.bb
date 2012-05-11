// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_view_host_delegate.h"

#include "googleurl/src/gurl.h"
#include "webkit/glue/webpreferences.h"

namespace content {

RenderViewHostDelegate::View* RenderViewHostDelegate::GetViewDelegate() {
  return NULL;
}

RenderViewHostDelegate::RendererManagement*
RenderViewHostDelegate::GetRendererManagementDelegate() {
  return NULL;
}

bool RenderViewHostDelegate::OnMessageReceived(const IPC::Message& message) {
  return false;
}

const GURL& RenderViewHostDelegate::GetURL() const {
  return GURL::EmptyGURL();
}

WebContents* RenderViewHostDelegate::GetAsWebContents() {
  return NULL;
}

webkit_glue::WebPreferences RenderViewHostDelegate::GetWebkitPrefs() {
  return webkit_glue::WebPreferences();
}

bool RenderViewHostDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  return false;
}

bool RenderViewHostDelegate::IsFullscreenForCurrentTab() const {
  return false;
}

}  // namespace content
