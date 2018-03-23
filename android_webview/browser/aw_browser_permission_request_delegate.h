// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PERMISSION_REQUEST_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PERMISSION_REQUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "url/gurl.h"

namespace android_webview {

// Delegate interface to handle the permission requests from |BrowserContext|.
class AwBrowserPermissionRequestDelegate {
 public:
  // Returns the AwBrowserPermissionRequestDelegate instance associated with
  // the given render_process_id and render_frame_id, or NULL.
  static AwBrowserPermissionRequestDelegate* FromID(int render_process_id,
                                                    int render_frame_id);

  virtual void RequestProtectedMediaIdentifierPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) = 0;

  virtual void CancelProtectedMediaIdentifierPermissionRequests(
      const GURL& origin) = 0;

  virtual void RequestGeolocationPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) = 0;

  virtual void CancelGeolocationPermissionRequests(const GURL& origin) = 0;

  virtual void RequestMIDISysexPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) = 0;

  virtual void CancelMIDISysexPermissionRequests(const GURL& origin) = 0;

 protected:
  AwBrowserPermissionRequestDelegate() {}
};

}  // namespace android_webview
#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PERMISSION_REQUEST_DELEGATE_H_
