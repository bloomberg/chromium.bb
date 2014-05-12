// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_geolocation_permission_context.h"

#include "android_webview/native/aw_contents.h"
#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

AwGeolocationPermissionContext::~AwGeolocationPermissionContext() {
}

void AwGeolocationPermissionContext::RequestGeolocationPermission(
    content::WebContents* web_contents,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> callback) {
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents) {
    callback.Run(false);
    return;
  }
  aw_contents->ShowGeolocationPrompt(requesting_frame, callback);
}

// static
content::GeolocationPermissionContext*
AwGeolocationPermissionContext::Create(AwBrowserContext* browser_context) {
  return new AwGeolocationPermissionContext();
}

void AwGeolocationPermissionContext::CancelGeolocationPermissionRequest(
    content::WebContents* web_contents,
    int bridge_id,
    const GURL& requesting_frame) {
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (aw_contents) {
    aw_contents->HideGeolocationPrompt(requesting_frame);
  }
}

}  // namespace android_webview
