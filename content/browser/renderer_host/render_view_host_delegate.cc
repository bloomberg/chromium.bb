// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate.h"

#include "googleurl/src/gurl.h"
#include "webkit/glue/webpreferences.h"

namespace content {

RenderViewHostDelegateView* RenderViewHostDelegate::GetDelegateView() {
  return NULL;
}

RenderViewHostDelegate::RendererManagement*
RenderViewHostDelegate::GetRendererManagementDelegate() {
  return NULL;
}

bool RenderViewHostDelegate::OnMessageReceived(RenderViewHost* render_view_host,
                                               const IPC::Message& message) {
  return false;
}

bool RenderViewHostDelegate::AddMessageToConsole(
    int32 level, const string16& message, int32 line_no,
    const string16& source_id) {
  return false;
}

const GURL& RenderViewHostDelegate::GetURL() const {
  return GURL::EmptyGURL();
}

WebContents* RenderViewHostDelegate::GetAsWebContents() {
  return NULL;
}

WebPreferences RenderViewHostDelegate::GetWebkitPrefs() {
  return WebPreferences();
}

bool RenderViewHostDelegate::IsFullscreenForCurrentTab() const {
  return false;
}

}  // namespace content
