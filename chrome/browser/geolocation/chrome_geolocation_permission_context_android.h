// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"

class GoogleLocationSettingsHelper;

// Android-specific geolocation permission flow, taking into account the
// Google Location Settings, if available.
class ChromeGeolocationPermissionContextAndroid
    : public ChromeGeolocationPermissionContext {
 public:
  explicit ChromeGeolocationPermissionContextAndroid(Profile* profile);

 private:
  friend class ChromeGeolocationPermissionContext;

  virtual ~ChromeGeolocationPermissionContextAndroid();

  // ChromeGeolocationPermissionContext implementation:
  virtual void DecidePermission(const GeolocationPermissionRequestID& id,
                                const GURL& requesting_frame,
                                const GURL& embedder,
                                base::Callback<void(bool)> callback) OVERRIDE;

  virtual void PermissionDecided(const GeolocationPermissionRequestID& id,
                                 const GURL& requesting_frame,
                                 const GURL& embedder,
                                 base::Callback<void(bool)> callback,
                                 bool allowed) OVERRIDE;

  scoped_ptr<GoogleLocationSettingsHelper> google_location_settings_helper_;

  DISALLOW_COPY_AND_ASSIGN(ChromeGeolocationPermissionContextAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
