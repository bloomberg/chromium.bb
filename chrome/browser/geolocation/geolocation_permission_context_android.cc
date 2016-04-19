// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/android/location_settings.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

GeolocationPermissionContextAndroid::
    GeolocationPermissionContextAndroid(Profile* profile)
    : GeolocationPermissionContext(profile),
      location_settings_(new LocationSettingsImpl()),
      permission_update_infobar_(nullptr),
      weak_factory_(this) {
}

GeolocationPermissionContextAndroid::~GeolocationPermissionContextAndroid() {
}

void GeolocationPermissionContextAndroid::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    const BrowserPermissionCallback& callback) {
  if (!location_settings_->CanSitesRequestLocationPermission(web_contents)) {
    PermissionDecided(id, requesting_frame_origin,
                      web_contents->GetLastCommittedURL().GetOrigin(),
                      callback, false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  ContentSetting content_setting =
      GeolocationPermissionContext::GetPermissionStatus(requesting_frame_origin,
                                                        embedding_origin);
  std::vector<ContentSettingsType> content_settings_types;
  content_settings_types.push_back(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  if (content_setting == CONTENT_SETTING_ALLOW &&
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfobar(
          web_contents, content_settings_types)) {
    permission_update_infobar_ = PermissionUpdateInfoBarDelegate::Create(
        web_contents, content_settings_types,
        base::Bind(
            &GeolocationPermissionContextAndroid
                ::HandleUpdateAndroidPermissions,
            weak_factory_.GetWeakPtr(), id, requesting_frame_origin,
            embedding_origin, callback));

    return;
  }

  GeolocationPermissionContext::RequestPermission(
      web_contents, id, requesting_frame_origin, callback);
}

void GeolocationPermissionContextAndroid::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  if (permission_update_infobar_) {
    permission_update_infobar_->RemoveSelf();
    permission_update_infobar_ = nullptr;
  }

  GeolocationPermissionContext::CancelPermissionRequest(web_contents, id);
}

void GeolocationPermissionContextAndroid::HandleUpdateAndroidPermissions(
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool permissions_updated) {
  permission_update_infobar_ = nullptr;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ContentSetting new_setting = permissions_updated
      ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;

  NotifyPermissionSet(id, requesting_frame_origin, embedding_origin, callback,
                      false /* persist */, new_setting);
}

void GeolocationPermissionContextAndroid::SetLocationSettingsForTesting(
    std::unique_ptr<LocationSettings> settings) {
  location_settings_ = std::move(settings);
}
