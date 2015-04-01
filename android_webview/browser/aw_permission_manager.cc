// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_permission_manager.h"

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "base/callback.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

namespace {

void CallbackPermisisonStatusWrapper(
    const base::Callback<void(content::PermissionStatus)>& callback,
    bool allowed) {
  callback.Run(allowed ? content::PERMISSION_STATUS_GRANTED
                       : content::PERMISSION_STATUS_DENIED);
}

}  // anonymous namespace

AwPermissionManager::AwPermissionManager()
    : content::PermissionManager() {
}

AwPermissionManager::~AwPermissionManager() {
}

void AwPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::WebContents* web_contents,
    int request_id,
    const GURL& origin,
    bool user_gesture,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (!delegate) {
    DVLOG(0) << "Dropping permission request for "
             << static_cast<int>(permission);
    callback.Run(content::PERMISSION_STATUS_DENIED);
    return;
  }

  switch (permission) {
    case content::PermissionType::GEOLOCATION:
      delegate->RequestGeolocationPermission(
          origin, base::Bind(&CallbackPermisisonStatusWrapper, callback));
      break;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      delegate->RequestProtectedMediaIdentifierPermission(
          origin, base::Bind(&CallbackPermisisonStatusWrapper, callback));
      break;
    case content::PermissionType::MIDI_SYSEX:
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      NOTIMPLEMENTED() << "RequestPermission is not implemented for "
                       << static_cast<int>(permission);
      callback.Run(content::PERMISSION_STATUS_DENIED);
      break;
    case content::PermissionType::NUM:
      NOTREACHED() << "PermissionType::NUM was not expected here.";
      callback.Run(content::PERMISSION_STATUS_DENIED);
      break;
  }
}

void AwPermissionManager::CancelPermissionRequest(
    content::PermissionType permission,
    content::WebContents* web_contents,
    int request_id,
    const GURL& origin) {
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (!delegate)
    return;

  switch (permission) {
    case content::PermissionType::GEOLOCATION:
      delegate->CancelGeolocationPermissionRequests(origin);
      break;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      delegate->CancelProtectedMediaIdentifierPermissionRequests(origin);
      break;
    case content::PermissionType::MIDI_SYSEX:
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      NOTIMPLEMENTED() << "CancelPermission not implemented for "
                       << static_cast<int>(permission);
      break;
    case content::PermissionType::NUM:
      NOTREACHED() << "PermissionType::NUM was not expected here.";
      break;
  }
}

void AwPermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

content::PermissionStatus AwPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return content::PERMISSION_STATUS_DENIED;
}

void AwPermissionManager::RegisterPermissionUsage(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

int AwPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  return -1;
}

void AwPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace android_webview
