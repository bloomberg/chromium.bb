// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class GoogleLocationSettingsHelper;

// Android-specific geolocation permission flow, taking into account the
// Google Location Settings, if available.
class GeolocationPermissionContextAndroid
    : public GeolocationPermissionContext {
 public:
  explicit GeolocationPermissionContextAndroid(Profile* profile);

 private:
  struct PermissionRequestInfo {
    PermissionRequestInfo();

    PermissionRequestID id;
    GURL requesting_frame;
    bool user_gesture;
    GURL embedder;
  };

  friend class GeolocationPermissionContext;

  virtual ~GeolocationPermissionContextAndroid();

  // GeolocationPermissionContext implementation:
  virtual void DecidePermission(content::WebContents* web_contents,
                                const PermissionRequestID& id,
                                const GURL& requesting_frame,
                                bool user_gesture,
                                const GURL& embedder,
                                const std::string& accept_button_label,
                                base::Callback<void(bool)> callback) OVERRIDE;

  virtual void PermissionDecided(const PermissionRequestID& id,
                                 const GURL& requesting_frame,
                                 const GURL& embedder,
                                 base::Callback<void(bool)> callback,
                                 bool allowed) OVERRIDE;

  void ProceedDecidePermission(content::WebContents* web_contents,
                               const PermissionRequestInfo& info,
                               const std::string& accept_button_label,
                               base::Callback<void(bool)> callback);

  scoped_ptr<GoogleLocationSettingsHelper> google_location_settings_helper_;

 private:
  void CheckMasterLocation(content::WebContents* web_contents,
                           const PermissionRequestInfo& info,
                           base::Callback<void(bool)> callback);

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
