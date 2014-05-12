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
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> callback) OVERRIDE;
  virtual void CancelGeolocationPermissionRequest(
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE;

 protected:
  virtual ~AwGeolocationPermissionContext();

 private:
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_GEOLOCATION_PERMISSION_CONTEXT_H_
