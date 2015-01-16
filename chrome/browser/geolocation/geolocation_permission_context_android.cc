// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include "chrome/browser/android/app_google_location_settings_helper.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

GeolocationPermissionContextAndroid::
    GeolocationPermissionContextAndroid(Profile* profile)
    : GeolocationPermissionContext(profile),
      google_location_settings_helper_(new AppGoogleLocationSettingsHelper()) {
}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() {
}

void GeolocationPermissionContextAndroid::RequestPermission(
    content::WebContents* web_contents,
     const PermissionRequestID& id,
     const GURL& requesting_frame_origin,
     bool user_gesture,
     const BrowserPermissionCallback& callback) {
  if (!google_location_settings_helper_->IsSystemLocationEnabled()) {
    PermissionDecided(id, requesting_frame_origin,
                      web_contents->GetLastCommittedURL().GetOrigin(),
                      callback, false /* persist */, false /* granted */);
    return;
  }

  GeolocationPermissionContext::RequestPermission(
      web_contents, id, requesting_frame_origin, user_gesture, callback);
}

void GeolocationPermissionContextAndroid::
    SetGoogleLocationSettingsHelperForTesting(
        scoped_ptr<GoogleLocationSettingsHelper> helper) {
  google_location_settings_helper_ = helper.Pass();
}
