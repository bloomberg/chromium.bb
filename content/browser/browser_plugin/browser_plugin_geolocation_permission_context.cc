// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_geolocation_permission_context.h"

#include "base/bind.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

namespace content {

BrowserPluginGeolocationPermissionContext::
    BrowserPluginGeolocationPermissionContext() {
}

BrowserPluginGeolocationPermissionContext::
    ~BrowserPluginGeolocationPermissionContext() {
}

void BrowserPluginGeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &BrowserPluginGeolocationPermissionContext::
                RequestGeolocationPermission,
            this,
            render_process_id,
            render_view_id,
            bridge_id,
            requesting_frame,
            user_gesture,
            callback));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Note that callback.Run(true) allows geolocation access, callback.Run(false)
  // denies geolocation access.
  // We need to go to the renderer to ask embedder's js if we are allowed to
  // have geolocation access.
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (rvh) {
    DCHECK(rvh->GetProcess()->IsGuest());
    WebContentsImpl* guest_web_contents =
        static_cast<WebContentsImpl*>(rvh->GetDelegate()->GetAsWebContents());
    BrowserPluginGuest* guest = guest_web_contents->GetBrowserPluginGuest();
    guest->AskEmbedderForGeolocationPermission(bridge_id,
                                               requesting_frame,
                                               user_gesture,
                                               callback);
  }
}

void BrowserPluginGeolocationPermissionContext::
    CancelGeolocationPermissionRequest(int render_process_id,
                                       int render_view_id,
                                       int bridge_id,
                                       const GURL& requesting_frame) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &BrowserPluginGeolocationPermissionContext::
                CancelGeolocationPermissionRequest,
            this,
            render_process_id,
            render_view_id,
            bridge_id,
            requesting_frame));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (rvh) {
    DCHECK(rvh->GetProcess()->IsGuest());
    WebContentsImpl* guest_web_contents =
        static_cast<WebContentsImpl*>(rvh->GetDelegate()->GetAsWebContents());
    BrowserPluginGuest* guest = guest_web_contents->GetBrowserPluginGuest();
    if (guest)
      guest->CancelGeolocationRequest(bridge_id);
  }
}

}  // namespace content
