// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context_android.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

ChromeGeolocationPermissionContextAndroid::
ChromeGeolocationPermissionContextAndroid(Profile* profile)
    : ChromeGeolocationPermissionContext(profile) {
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
  if (!profile()->GetPrefs()->GetBoolean(prefs::kGeolocationEnabled)) {
    PermissionDecided(render_process_id, render_view_id, bridge_id,
                      requesting_frame, embedder, callback, false);
    return;
  }

  ChromeGeolocationPermissionContext::DecidePermission(
      render_process_id, render_view_id, bridge_id,
      requesting_frame, embedder, callback);
}
