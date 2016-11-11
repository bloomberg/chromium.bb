// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_geolocation_disclosure_tab_helper.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/search_geolocation_disclosure_infobar_delegate.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "jni/GeolocationHeader_jni.h"

namespace {

const int kDefaultMaxShowCount = 3;
const int kDefaultDaysPerShow = 1;
const char kMaxShowCountVariation[] = "MaxShowCount";
const char kDaysPerShowVariation[] = "DaysPerShow";

int GetMaxShowCount() {
  std::string variation = variations::GetVariationParamValueByFeature(
      features::kConsistentOmniboxGeolocation, kMaxShowCountVariation);
  int max_show;
  if (!variation.empty() && base::StringToInt(variation, &max_show))
    return max_show;

  return kDefaultMaxShowCount;
}

int GetDaysPerShow() {
  std::string variation = variations::GetVariationParamValueByFeature(
      features::kConsistentOmniboxGeolocation, kDaysPerShowVariation);
  int days_per_show;
  if (!variation.empty() && base::StringToInt(variation, &days_per_show))
    return days_per_show;

  return kDefaultDaysPerShow;
}

}  // namespace

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

// static
void SearchGeolocationDisclosureTabHelper::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSearchGeolocationDisclosureDismissed,
                                false);
  registry->RegisterIntegerPref(prefs::kSearchGeolocationDisclosureShownCount,
                                0);
  registry->RegisterDoublePref(prefs::kSearchGeolocationDisclosureLastShowDate,
                               0);
}

void SearchGeolocationDisclosureTabHelper::
    MaybeShowDefaultSearchGeolocationDisclosure(GURL gurl) {
  // Don't show the infobar if the user has dismissed it.
  PrefService* prefs = GetProfile()->GetPrefs();
  bool dismissed_already =
      prefs->GetBoolean(prefs::kSearchGeolocationDisclosureDismissed);
  if (dismissed_already)
    return;

  // Or if they've seen it enough times already.
  int shown_count =
      prefs->GetInteger(prefs::kSearchGeolocationDisclosureShownCount);
  if (shown_count >= GetMaxShowCount())
    return;

  // Or if it has been shown too recently.
  base::Time last_shown = base::Time::FromInternalValue(
      prefs->GetDouble(prefs::kSearchGeolocationDisclosureLastShowDate));
  if (base::Time::Now() - last_shown <
      base::TimeDelta::FromDays(GetDaysPerShow())) {
    return;
  }

  // Only show the disclosure for default search navigations from the omnibox.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
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

  // Check that the Chrome app has geolocation permission.
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

  // All good, let's show the disclosure and increment the shown count.
  SearchGeolocationDisclosureInfoBarDelegate::Create(web_contents(), gurl);
  shown_count++;
  prefs->SetInteger(prefs::kSearchGeolocationDisclosureShownCount, shown_count);
  prefs->SetDouble(prefs::kSearchGeolocationDisclosureLastShowDate,
                   base::Time::Now().ToInternalValue());
}

Profile* SearchGeolocationDisclosureTabHelper::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}
