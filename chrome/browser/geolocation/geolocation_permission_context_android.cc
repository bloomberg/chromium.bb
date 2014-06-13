// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/android/google_location_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

GeolocationPermissionContextAndroid::
PermissionRequestInfo::PermissionRequestInfo()
    : id(0, 0, 0, GURL()),
      user_gesture(false) {}

GeolocationPermissionContextAndroid::
    GeolocationPermissionContextAndroid(Profile* profile)
    : GeolocationPermissionContext(profile),
      google_location_settings_helper_(
          GoogleLocationSettingsHelper::Create()) {
}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() {
}

void GeolocationPermissionContextAndroid::ProceedDecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestInfo& info,
    const std::string& accept_button_label,
    base::Callback<void(bool)> callback) {
  // Super class implementation expects everything in UI thread instead.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  GeolocationPermissionContext::DecidePermission(
      web_contents, info.id, info.requesting_frame, info.user_gesture,
      info.embedder, accept_button_label, callback);
}

void GeolocationPermissionContextAndroid::CheckMasterLocation(
    content::WebContents* web_contents,
    const PermissionRequestInfo& info,
    base::Callback<void(bool)> callback) {
  // Check to see if the feature in its entirety has been disabled.
  // This must happen before other services (e.g. tabs, extensions)
  // get an opportunity to allow the geolocation request.
  bool enabled =
      google_location_settings_helper_->IsMasterLocationSettingEnabled();

  // The flow for geolocation permission on android is:
  // - GeolocationPermissionContextAndroid::DecidePermission
  // intercepts the flow in the UI thread, and posts task
  // to the blocking pool to CheckMasterLocation (in order to
  // avoid strict-mode violation).
  // - At this point the master location permission is either:
  // -- enabled, in which we case it proceeds the normal flow
  // via GeolocationPermissionContext (which may create infobars, etc.).
  // -- disabled, in which case the permission is already decided.
  //
  // In either case, GeolocationPermissionContext expects these
  // in the UI thread.
  base::Closure ui_closure;
  if (enabled) {
    bool allow_label = google_location_settings_helper_->IsAllowLabel();
    std::string accept_button_label =
        google_location_settings_helper_->GetAcceptButtonLabel(allow_label);
    ui_closure = base::Bind(
        &GeolocationPermissionContextAndroid::ProceedDecidePermission,
        this, web_contents, info, accept_button_label, callback);
  } else {
    ui_closure = base::Bind(
        &GeolocationPermissionContextAndroid::PermissionDecided,
        this, info.id, info.requesting_frame, info.embedder, callback, false);
  }

  // This method is executed from the BlockingPool, post the result
  // back to the UI thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, ui_closure);
}

void GeolocationPermissionContextAndroid::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    const GURL& embedder,
    const std::string& accept_button_label,
    base::Callback<void(bool)> callback) {

  PermissionRequestInfo info;
  info.id = id;
  info.requesting_frame = requesting_frame;
  info.user_gesture = user_gesture;
  info.embedder = embedder;

  // Called on the UI thread. However, do the work on a separate thread
  // to avoid strict mode violation.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(
          &GeolocationPermissionContextAndroid::CheckMasterLocation,
          this, web_contents, info, callback));
}

void GeolocationPermissionContextAndroid::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed) {
  // If Google Apps Location setting is turned off then we ignore
  // the 'allow' website setting for this site and instead show
  // the infobar to go back to the 'settings' to turn it back on.
  if (allowed &&
      !google_location_settings_helper_->IsGoogleAppsLocationSettingEnabled()) {
    QueueController()->CreateInfoBarRequest(
        id, requesting_frame, embedder, "", callback);
    return;
  }

  GeolocationPermissionContext::PermissionDecided(
      id, requesting_frame, embedder, callback, allowed);
}
