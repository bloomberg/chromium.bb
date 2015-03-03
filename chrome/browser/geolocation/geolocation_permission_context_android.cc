// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include "chrome/browser/android/location_settings.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

GeolocationPermissionContextAndroid::
    GeolocationPermissionContextAndroid(Profile* profile)
    : GeolocationPermissionContext(profile),
      location_settings_(new LocationSettingsImpl()) {
}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() {
}

void GeolocationPermissionContextAndroid::RequestPermission(
    content::WebContents* web_contents,
     const PermissionRequestID& id,
     const GURL& requesting_frame_origin,
     bool user_gesture,
     const BrowserPermissionCallback& callback) {
  if (!location_settings_->IsLocationEnabled()) {
    PermissionDecided(id, requesting_frame_origin,
                      web_contents->GetLastCommittedURL().GetOrigin(),
                      callback, false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  GeolocationPermissionContext::RequestPermission(
      web_contents, id, requesting_frame_origin, user_gesture, callback);
}

void GeolocationPermissionContextAndroid::SetLocationSettingsForTesting(
    scoped_ptr<LocationSettings> settings) {
  location_settings_ = settings.Pass();
}
