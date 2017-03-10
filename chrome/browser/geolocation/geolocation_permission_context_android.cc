// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_android.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/android/location_settings.h"
#include "chrome/browser/android/location_settings_impl.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_service.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/infobars/core/infobar.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
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
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  ContentSetting value =
      GeolocationPermissionContext::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);

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
  if (!IsLocationAccessPossible(web_contents, requesting_frame_origin,
                                user_gesture)) {
    PermissionDecided(id, requesting_frame_origin,
                      web_contents->GetLastCommittedURL().GetOrigin(),
                      user_gesture, callback, false /* persist */,
                      CONTENT_SETTING_BLOCK);
    return;
  }

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  ContentSetting content_setting =
      GeolocationPermissionContext::GetPermissionStatus(
          nullptr /* render_frame_host */, requesting_frame_origin,
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
  if (content_setting == CONTENT_SETTING_ALLOW &&
      !location_settings_->IsSystemLocationSettingEnabled()) {
    // There is no need to check CanShowLocationSettingsDialog here again, as it
    // must have been checked to get this far.
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(id.render_process_id(),
                                             id.render_frame_id()));
    location_settings_->PromptToEnableSystemLocationSetting(
        GetLocationSettingsDialogContext(requesting_origin), web_contents,
        base::BindOnce(
            &GeolocationPermissionContextAndroid::OnLocationSettingsDialogShown,
            weak_factory_.GetWeakPtr(), id, requesting_origin, embedding_origin,
            callback, persist, content_setting));
    return;
  }

  FinishNotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                            persist, content_setting);
}

PermissionResult
GeolocationPermissionContextAndroid::UpdatePermissionStatusWithDeviceStatus(
    PermissionResult result,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (base::FeatureList::IsEnabled(features::kLsdPermissionPrompt) &&
      result.content_setting != CONTENT_SETTING_BLOCK) {
    if (!location_settings_->IsSystemLocationSettingEnabled()) {
      // As this is returning the status for possible future permission
      // requests, whose gesture status is unknown, pretend there is a user
      // gesture here. If there is a possibility of PROMPT (i.e. if there is a
      // user gesture attached to the later request) that should be returned,
      // not BLOCK.
      if (CanShowLocationSettingsDialog(requesting_origin,
                                        true /* user_gesture */)) {
        result.content_setting = CONTENT_SETTING_ASK;
      } else {
        result.content_setting = CONTENT_SETTING_BLOCK;
      }
      result.source = PermissionStatusSource::UNSPECIFIED;
    }

    if (result.content_setting != CONTENT_SETTING_BLOCK &&
        !location_settings_->HasAndroidLocationPermission()) {
      // TODO(benwells): plumb through the RFH and use the associated
      // WebContents to check that the android location can be prompted for.
      result.content_setting = CONTENT_SETTING_ASK;
      result.source = PermissionStatusSource::UNSPECIFIED;
    }
  }

  return result;
}

bool GeolocationPermissionContextAndroid::IsLocationAccessPossible(
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    bool user_gesture) {
  return (location_settings_->HasAndroidLocationPermission() ||
          location_settings_->CanPromptForAndroidLocationPermission(
              web_contents)) &&
         (location_settings_->IsSystemLocationSettingEnabled() ||
          CanShowLocationSettingsDialog(requesting_origin, user_gesture));
}

LocationSettingsDialogContext
GeolocationPermissionContextAndroid::GetLocationSettingsDialogContext(
    const GURL& requesting_origin) const {
  bool is_dse_origin = false;
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  if (template_url_service) {
    const TemplateURL* template_url =
        template_url_service->GetDefaultSearchProvider();
    if (template_url) {
      GURL search_url = template_url->GenerateSearchURL(
          template_url_service->search_terms_data());
      is_dse_origin = url::IsSameOriginWith(requesting_origin, search_url);
    }
  }

  return is_dse_origin ? SEARCH : DEFAULT;
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

bool GeolocationPermissionContextAndroid::CanShowLocationSettingsDialog(
    const GURL& requesting_origin,
    bool user_gesture) const {
  if (!base::FeatureList::IsEnabled(features::kLsdPermissionPrompt))
    return false;

  // If this isn't the default search engine, a gesture is needed.
  if (GetLocationSettingsDialogContext(requesting_origin) == DEFAULT &&
      !user_gesture) {
    return false;
  }

  // TODO(benwells): Also check backoff for |requesting_origin|.
  return location_settings_->CanPromptToEnableSystemLocationSetting();
}

void GeolocationPermissionContextAndroid::OnLocationSettingsDialogShown(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting,
    LocationSettingsDialogOutcome prompt_outcome) {
  if (prompt_outcome != GRANTED) {
    content_setting = CONTENT_SETTING_BLOCK;
    persist = false;
  }

  FinishNotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                            persist, content_setting);
}

void GeolocationPermissionContextAndroid::FinishNotifyPermissionSet(
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
    if (!web_contents)
      return;

    SearchGeolocationDisclosureTabHelper* disclosure_helper =
        SearchGeolocationDisclosureTabHelper::FromWebContents(web_contents);

    // The tab helper can be null in tests.
    if (disclosure_helper)
      disclosure_helper->MaybeShowDisclosure(requesting_origin);
  }
}

void GeolocationPermissionContextAndroid::SetLocationSettingsForTesting(
    std::unique_ptr<LocationSettings> settings) {
  location_settings_ = std::move(settings);
}
