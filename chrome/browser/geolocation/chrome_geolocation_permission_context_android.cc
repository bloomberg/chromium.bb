// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context_android.h"

#include "chrome/browser/android/google_location_settings_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

ChromeGeolocationPermissionContextAndroid::
ChromeGeolocationPermissionContextAndroid(Profile* profile)
    : ChromeGeolocationPermissionContext(profile),
      google_location_settings_helper_(new GoogleLocationSettingsHelper()) {
}

ChromeGeolocationPermissionContextAndroid::
    ~ChromeGeolocationPermissionContextAndroid() {
}

void ChromeGeolocationPermissionContextAndroid::DecidePermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Check to see if the feature in its entirety has been disabled.
  // This must happen before other services (e.g. tabs, extensions)
  // get an opportunity to allow the geolocation request.
  if (!google_location_settings_helper_->IsMasterLocationSettingEnabled()) {
    PermissionDecided(render_process_id, render_view_id, bridge_id,
                      requesting_frame, embedder, callback, false);
    return;
  }

  ChromeGeolocationPermissionContext::DecidePermission(
      render_process_id, render_view_id, bridge_id,
      requesting_frame, embedder, callback);
}

void ChromeGeolocationPermissionContextAndroid::PermissionDecided(
    int render_process_id,
    int render_view_id,
    int bridge_id,
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
    QueueController()->CreateInfoBarRequest(
        render_process_id, render_view_id, bridge_id, requesting_frame,
        embedder, callback);
    return;
  }

  ChromeGeolocationPermissionContext::PermissionDecided(
      render_process_id, render_view_id, bridge_id,
      requesting_frame, embedder, callback, allowed);
}
