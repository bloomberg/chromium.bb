// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

#include "base/callback.h"

namespace content {

bool BrowserPluginGuestDelegate::IsDragAndDropEnabled() {
  return false;
}

void BrowserPluginGuestDelegate::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
  callback.Run(MediaStreamDevices(),
               MEDIA_DEVICE_INVALID_STATE,
               scoped_ptr<MediaStreamUI>());
}

void BrowserPluginGuestDelegate::CanDownload(
    const std::string& request_method,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

JavaScriptDialogManager*
BrowserPluginGuestDelegate::GetJavaScriptDialogManager() {
  return NULL;
}

ColorChooser* BrowserPluginGuestDelegate::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<ColorSuggestion>& suggestions) {
  return NULL;
}

bool BrowserPluginGuestDelegate::HandleContextMenu(
    const ContextMenuParams& params) {
  return false;
}

WebContents* BrowserPluginGuestDelegate::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  return NULL;
}

bool BrowserPluginGuestDelegate::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  return false;
}

}  // namespace content
