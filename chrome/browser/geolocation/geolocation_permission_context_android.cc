// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

GeolocationPermissionContextAndroid::
PermissionRequestInfo::PermissionRequestInfo()
    : id(0, 0, 0, GURL()),
      user_gesture(false) {
}

GeolocationPermissionContextAndroid::
    GeolocationPermissionContextAndroid(Profile* profile)
    : GeolocationPermissionContext(profile),
      google_location_settings_helper_(
          GoogleLocationSettingsHelper::Create()),
      weak_factory_(this) {
}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() {
}

void GeolocationPermissionContextAndroid::ProceedDecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestInfo& info,
    base::Callback<void(bool)> callback) {

  // The super class implementation expects everything in UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GeolocationPermissionContext::RequestPermission(
      web_contents, info.id,
      info.requesting_origin,
      info.user_gesture,
      callback);
}


// UI thread
void GeolocationPermissionContextAndroid::RequestPermission(
    content::WebContents* web_contents,
     const PermissionRequestID& id,
     const GURL& requesting_frame_origin,
     bool user_gesture,
     const BrowserPermissionCallback& callback) {
  PermissionRequestInfo info;
  info.id = id;
  info.requesting_origin = requesting_frame_origin;
  info.embedder_origin = web_contents->GetLastCommittedURL().GetOrigin();
  info.user_gesture = user_gesture;

  // Called on the UI thread. However, do the work on a separate thread
  // to avoid strict mode violation.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(
          &GeolocationPermissionContextAndroid::CheckMasterLocation,
          weak_factory_.GetWeakPtr(), web_contents, info, callback));
}

// Blocking pool thread
void GeolocationPermissionContextAndroid::CheckMasterLocation(
    content::WebContents* web_contents,
    const PermissionRequestInfo& info,
    const BrowserPermissionCallback& callback) {
  // Check to see if the feature in its entirety has been disabled.
  // This must happen before other services (e.g. tabs, extensions)
  // get an opportunity to allow the geolocation request.
  bool enabled =
      google_location_settings_helper_->IsSystemLocationEnabled();

  base::Closure ui_closure;
  if (enabled) {
    ui_closure = base::Bind(
        &GeolocationPermissionContextAndroid::ProceedDecidePermission,
        base::Unretained(this), web_contents, info, callback);
  } else {
    ui_closure = base::Bind(
        &GeolocationPermissionContextAndroid::PermissionDecided,
        base::Unretained(this), info.id, info.requesting_origin,
        info.embedder_origin, callback, false, false);
  }

  // This method is executed from the BlockingPool, post the result
  // back to the UI thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, ui_closure);
}


void GeolocationPermissionContextAndroid::InterceptPermissionCheck(
    const BrowserPermissionCallback& callback, bool granted) {
  callback.Run(granted);
}
