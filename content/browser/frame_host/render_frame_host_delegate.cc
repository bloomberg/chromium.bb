// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

bool RenderFrameHostDelegate::OnMessageReceived(
    RenderFrameHost* render_view_host,
    const IPC::Message& message) {
  return false;
}

const GURL& RenderFrameHostDelegate::GetMainFrameLastCommittedURL() const {
  return GURL::EmptyGURL();
}

bool RenderFrameHostDelegate::WillHandleDeferAfterResponseStarted() {
  return false;
}

bool RenderFrameHostDelegate::AddMessageToConsole(
    int32 level, const base::string16& message, int32 line_no,
    const base::string16& source_id) {
  return false;
}

WebContents* RenderFrameHostDelegate::GetAsWebContents() {
  return NULL;
}

void RenderFrameHostDelegate::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
  LOG(ERROR) << "RenderFrameHostDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  callback.Run(MediaStreamDevices(),
               MEDIA_DEVICE_NOT_SUPPORTED,
               scoped_ptr<MediaStreamUI>());
}

bool RenderFrameHostDelegate::CheckMediaAccessPermission(
    const GURL& security_origin,
    MediaStreamType type) {
  LOG(ERROR) << "RenderFrameHostDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

AccessibilityMode RenderFrameHostDelegate::GetAccessibilityMode() const {
  return AccessibilityModeOff;
}

RenderFrameHost* RenderFrameHostDelegate::GetGuestByInstanceID(
    int browser_plugin_instance_id) {
  return NULL;
}

#if defined(OS_WIN)
gfx::NativeViewAccessible
    RenderFrameHostDelegate::GetParentNativeViewAccessible() {
  return NULL;
}
#endif  // defined(OS_WIN)

}  // namespace content
