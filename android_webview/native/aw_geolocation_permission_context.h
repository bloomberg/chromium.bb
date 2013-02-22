// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_GEOLOCATION_PERMISSION_CONTEXT_H_
#define ANDROID_WEBVIEW_NATIVE_AW_GEOLOCATION_PERMISSION_CONTEXT_H_

#include "content/public/browser/geolocation_permission_context.h"

class GURL;

namespace android_webview {

class AwBrowserContext;

class AwGeolocationPermissionContext :
    public content::GeolocationPermissionContext {
 public:
  static content::GeolocationPermissionContext* Create(
      AwBrowserContext* browser_context);

  // content::GeolocationPermissionContext implementation
  virtual void RequestGeolocationPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      base::Callback<void(bool)> callback) OVERRIDE;
  virtual void CancelGeolocationPermissionRequest(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE;

 protected:
  virtual ~AwGeolocationPermissionContext();

 private:
  void RequestGeolocationPermissionOnUIThread(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      base::Callback<void(bool)> callback);

  void CancelGeolocationPermissionRequestOnUIThread(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_GEOLOCATION_PERMISSION_CONTEXT_H_
