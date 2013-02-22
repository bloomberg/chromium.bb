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

void
AwGeolocationPermissionContext::RequestGeolocationPermissionOnUIThread(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    base::Callback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  AwContents* aw_contents =
      AwContents::FromID(render_process_id, render_view_id);
  if (!aw_contents) {
    callback.Run(false);
    return;
  }
  aw_contents->ShowGeolocationPrompt(requesting_frame, callback);
}

void
AwGeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    base::Callback<void(bool)> callback) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(
          &AwGeolocationPermissionContext::
              RequestGeolocationPermissionOnUIThread,
          this,
          render_process_id,
          render_view_id,
          bridge_id,
          requesting_frame,
          callback));
}

// static
content::GeolocationPermissionContext*
AwGeolocationPermissionContext::Create(AwBrowserContext* browser_context) {
  return new AwGeolocationPermissionContext();
}

void
AwGeolocationPermissionContext::CancelGeolocationPermissionRequestOnUIThread(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  AwContents* aw_contents =
      AwContents::FromID(render_process_id, render_view_id);
  if (aw_contents) {
    aw_contents->HideGeolocationPrompt(requesting_frame);
  }
}

void
AwGeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(
          &AwGeolocationPermissionContext::
              CancelGeolocationPermissionRequestOnUIThread,
          this,
          render_process_id,
          render_view_id,
          bridge_id,
          requesting_frame));
}

}  // namespace android_webview
