// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

bool RenderFrameHostDelegate::OnMessageReceived(
    RenderFrameHostImpl* render_frame_host,
    const IPC::Message& message) {
  return false;
}

const GURL& RenderFrameHostDelegate::GetMainFrameLastCommittedURL() const {
  return GURL::EmptyGURL();
}

bool RenderFrameHostDelegate::DidAddMessageToConsole(
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return false;
}

WebContents* RenderFrameHostDelegate::GetAsWebContents() {
  return NULL;
}

InterstitialPage* RenderFrameHostDelegate::GetAsInterstitialPage() {
  return nullptr;
}

void RenderFrameHostDelegate::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
  LOG(ERROR) << "RenderFrameHostDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  callback.Run(MediaStreamDevices(), MEDIA_DEVICE_NOT_SUPPORTED,
               std::unique_ptr<MediaStreamUI>());
}

bool RenderFrameHostDelegate::CheckMediaAccessPermission(
    const GURL& security_origin,
    MediaStreamType type) {
  LOG(ERROR) << "RenderFrameHostDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

std::string RenderFrameHostDelegate::GetDefaultMediaDeviceID(
    MediaStreamType type) {
  return std::string();
}

AccessibilityMode RenderFrameHostDelegate::GetAccessibilityMode() const {
  return AccessibilityMode();
}

RenderFrameHost* RenderFrameHostDelegate::GetGuestByInstanceID(
    RenderFrameHost* render_frame_host,
    int browser_plugin_instance_id) {
  return NULL;
}

device::GeolocationServiceContext*
RenderFrameHostDelegate::GetGeolocationServiceContext() {
  return nullptr;
}

device::mojom::WakeLockService* RenderFrameHostDelegate::GetRendererWakeLock() {
  return nullptr;
}

#if defined(OS_ANDROID)
void RenderFrameHostDelegate::GetNFC(device::mojom::NFCRequest request) {}
#endif

bool RenderFrameHostDelegate::ShouldRouteMessageEvent(
    RenderFrameHost* target_rfh,
    SiteInstance* source_site_instance) const {
  return false;
}

std::unique_ptr<WebUIImpl>
RenderFrameHostDelegate::CreateWebUIForRenderFrameHost(const GURL& url) {
  return nullptr;
}

bool RenderFrameHostDelegate::ShouldAllowRunningInsecureContent(
    WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  return false;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostDelegate::GetJavaRenderFrameHostDelegate() {
  return nullptr;
}
#endif

bool RenderFrameHostDelegate::IsBeingDestroyed() const {
  return false;
}

}  // namespace content
