// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_geolocation_disclosure_tab_helper.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/android/search_geolocation_disclosure_infobar_delegate.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "jni/GeolocationHeader_jni.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SearchGeolocationDisclosureTabHelper);

SearchGeolocationDisclosureTabHelper::SearchGeolocationDisclosureTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  consistent_geolocation_enabled_ =
      base::FeatureList::IsEnabled(features::kConsistentOmniboxGeolocation);
}

SearchGeolocationDisclosureTabHelper::~SearchGeolocationDisclosureTabHelper() {}

void SearchGeolocationDisclosureTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (consistent_geolocation_enabled_) {
    MaybeShowDefaultSearchGeolocationDisclosure(
        web_contents()->GetVisibleURL());
  }
}

void SearchGeolocationDisclosureTabHelper::
    MaybeShowDefaultSearchGeolocationDisclosure(GURL gurl) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  // Only show the disclosure for default search navigations from the omnibox.
  bool is_search_url =
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          gurl);
  if (!is_search_url)
    return;

  // Only show the disclosure if Google is the default search engine.
  TemplateURL* default_search =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search ||
      !default_search->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data())) {
    return;
  }

  // Check that the Chrome app has geolocatin permission.
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_GeolocationHeader_hasGeolocationPermission(env))
    return;

  // Only show the disclosure if the geolocation permission is set to ASK
  // (i.e. has not been explicitly set or revoked).
  blink::mojom::PermissionStatus status =
      PermissionManager::Get(GetProfile())
          ->GetPermissionStatus(content::PermissionType::GEOLOCATION, gurl,
                                gurl);
  if (status != blink::mojom::PermissionStatus::ASK)
    return;

  // All good, let's show the disclosure.
  SearchGeolocationDisclosureInfoBarDelegate::Create(web_contents(), gurl);
}

Profile* SearchGeolocationDisclosureTabHelper::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}
