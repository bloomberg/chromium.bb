// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context_android.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

ChromeGeolocationPermissionContextAndroid::
    ChromeGeolocationPermissionContextAndroid(Profile* profile)
    : ChromeGeolocationPermissionContext(profile),
      google_location_settings_helper_(
          GoogleLocationSettingsHelper::Create()) {
}

ChromeGeolocationPermissionContextAndroid::
    ~ChromeGeolocationPermissionContextAndroid() {
}

void ChromeGeolocationPermissionContextAndroid::DecidePermission(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Check to see if the feature in its entirety has been disabled.
  // This must happen before other services (e.g. tabs, extensions)
  // get an opportunity to allow the geolocation request.
  if (!google_location_settings_helper_->IsMasterLocationSettingEnabled()) {
    PermissionDecided(id, requesting_frame, embedder, callback, false);
    return;
  }

  ChromeGeolocationPermissionContext::DecidePermission(id, requesting_frame,
                                                       embedder, callback);
}

void ChromeGeolocationPermissionContextAndroid::PermissionDecided(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If Google Apps Location setting is turned off then we ignore
  // the 'allow' website setting for this site and instead show
  // the infobar to go back to the 'settings' to turn it back on.
  if (allowed &&
      !google_location_settings_helper_->IsGoogleAppsLocationSettingEnabled()) {
    QueueController()->CreateInfoBarRequest(id, requesting_frame, embedder,
                                            callback);
    return;
  }

  ChromeGeolocationPermissionContext::PermissionDecided(
      id, requesting_frame, embedder, callback, allowed);
}
