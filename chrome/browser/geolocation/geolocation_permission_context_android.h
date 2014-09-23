// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

// The flow for geolocation permissions on Android needs to take into account
// the global geolocation settings so it differs from the desktop one. It
// works as follows.
// GeolocationPermissionContextAndroid::DecidePermission intercepts the flow in
// the UI thread, and posts a task to the blocking pool to CheckSystemLocation.
// CheckSystemLocation will in fact check several possible settings
//     - The global system geolocation setting
//     - The Google location settings on pre KK devices
//     - An old internal Chrome setting on pre-JB MR1 devices
// With all that information it will decide if system location is enabled.
// If enabled, it proceeds with the per site flow via
// GeolocationPermissionContext (which will check per site permissions, create
// infobars, etc.).
//
// Otherwise the permission is already decided.

// There is a bit of thread jumping since some of the permissions (like the
// per site settings) are queried on the UI thread while the system level
// permissions are considered I/O and thus checked in the blocking thread pool.

#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class GoogleLocationSettingsHelper;


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
                                base::Callback<void(bool)> callback) OVERRIDE;

  void ProceedDecidePermission(content::WebContents* web_contents,
                               const PermissionRequestInfo& info,
                               base::Callback<void(bool)> callback);

  scoped_ptr<GoogleLocationSettingsHelper> google_location_settings_helper_;

 private:
  void CheckSystemLocation(content::WebContents* web_contents,
                           const PermissionRequestInfo& info,
                           base::Callback<void(bool)> callback);

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
