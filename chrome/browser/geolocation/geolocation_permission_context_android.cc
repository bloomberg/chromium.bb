// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/android/location_settings.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_service.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
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

ContentSetting GeolocationPermissionContextAndroid::GetPermissionStatusInternal(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  ContentSetting value =
      GeolocationPermissionContext::GetPermissionStatusInternal(
          requesting_origin, embedding_origin);

  if (value == CONTENT_SETTING_ASK && requesting_origin == embedding_origin) {
    // Consult the DSE Geolocation setting. Note that this only needs to be
    // consulted when the content setting is ASK. In the other cases (ALLOW or
    // BLOCK) checking the setting is redundant, as the setting is kept
    // consistent with the content setting.
    SearchGeolocationService* search_helper =
        SearchGeolocationService::Factory::GetForBrowserContext(profile());

    // If the user is incognito, use the DSE Geolocation setting from the
    // original profile - but only if it is BLOCK.
    if (!search_helper) {
      DCHECK(profile()->IsOffTheRecord());
      search_helper = SearchGeolocationService::Factory::GetForBrowserContext(
          profile()->GetOriginalProfile());
    }

    if (search_helper &&
        search_helper->UseDSEGeolocationSetting(
            url::Origin(embedding_origin))) {
      if (!search_helper->GetDSEGeolocationSetting()) {
        // If the DSE setting is off, always return BLOCK.
        value = CONTENT_SETTING_BLOCK;
      } else if (!profile()->IsOffTheRecord()) {
        // Otherwise, return ALLOW only if this is not incognito.
        value = CONTENT_SETTING_ALLOW;
      }
    }
  }

  return value;
}

void GeolocationPermissionContextAndroid::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  if (!location_settings_->CanSitesRequestLocationPermission(web_contents)) {
    PermissionDecided(id, requesting_frame_origin,
                      web_contents->GetLastCommittedURL().GetOrigin(),
                      user_gesture, callback, false /* persist */,
                      CONTENT_SETTING_BLOCK);
    return;
  }

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  ContentSetting content_setting =
      GeolocationPermissionContext::GetPermissionStatus(requesting_frame_origin,
                                                        embedding_origin)
          .content_setting;
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
      web_contents, id, requesting_frame_origin, user_gesture, callback);
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

void GeolocationPermissionContextAndroid::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting) {
  GeolocationPermissionContext::NotifyPermissionSet(id, requesting_origin,
                                                    embedding_origin, callback,
                                                    persist, content_setting);

  // If this is the default search origin, and the DSE Geolocation setting is
  // being used, potentially show the disclosure.
  if (requesting_origin == embedding_origin) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(id.render_process_id(),
                                             id.render_frame_id()));
    SearchGeolocationDisclosureTabHelper* disclosure_helper =
        SearchGeolocationDisclosureTabHelper::FromWebContents(web_contents);

    // The tab helper can be null in tests.
    if (disclosure_helper)
      disclosure_helper->MaybeShowDisclosure(requesting_origin);
  }
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
