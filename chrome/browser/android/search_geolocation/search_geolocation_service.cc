// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_geolocation/search_geolocation_service.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kIsGoogleSearchEngineKey[] = "is_google_search_engine";
const char kDSESettingKey[] = "dse_setting";
const char kDSENameKey[] = "dse_name";

// Default implementation of SearchEngineDelegate that is used for production
// code.
class SearchEngineDelegateImpl
    : public SearchGeolocationService::SearchEngineDelegate,
      public TemplateURLServiceObserver {
 public:
  explicit SearchEngineDelegateImpl(Profile* profile)
      : profile_(profile),
        template_url_service_(
            TemplateURLServiceFactory::GetForProfile(profile_)) {
    if (template_url_service_)
      template_url_service_->AddObserver(this);
  }

  ~SearchEngineDelegateImpl() override {
    if (template_url_service_)
      template_url_service_->RemoveObserver(this);
  }

  base::string16 GetDSEName() override {
    if (template_url_service_) {
      const TemplateURL* template_url =
          template_url_service_->GetDefaultSearchProvider();
      if (template_url)
        return template_url->short_name();
    }

    return base::string16();
  }

  url::Origin GetDSEOrigin() override {
    if (template_url_service_) {
      const TemplateURL* template_url =
          template_url_service_->GetDefaultSearchProvider();
      if (template_url) {
        GURL search_url = template_url->GenerateSearchURL(
            template_url_service_->search_terms_data());
        return url::Origin(search_url);
      }
    }

    return url::Origin();
  }

  void SetDSEChangedCallback(const base::Closure& callback) override {
    dse_changed_callback_ = callback;
  }

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override { dse_changed_callback_.Run(); }

 private:
  Profile* profile_;

  // Will be null in unittests.
  TemplateURLService* template_url_service_;

  base::Closure dse_changed_callback_;
};

}  // namespace

struct SearchGeolocationService::PrefValue {
  base::string16 dse_name;
  bool setting = false;
};

// static
SearchGeolocationService*
SearchGeolocationService::Factory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SearchGeolocationService*>(GetInstance()
      ->GetServiceForBrowserContext(context, true));
}

// static
SearchGeolocationService::Factory*
SearchGeolocationService::Factory::GetInstance() {
  return base::Singleton<SearchGeolocationService::Factory>::get();
}

SearchGeolocationService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "SearchGeolocationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchGeolocationService::Factory::~Factory() {}

bool SearchGeolocationService::Factory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* SearchGeolocationService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SearchGeolocationService(Profile::FromBrowserContext(context));
}

void SearchGeolocationService::Factory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDSEGeolocationSetting);
  registry->RegisterDictionaryPref(
      prefs::kGoogleDSEGeolocationSettingDeprecated);
}

SearchGeolocationService::SearchGeolocationService(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile_)) {
  // This class should never be constructed in incognito.
  DCHECK(!profile_->IsOffTheRecord());

  if (!UseConsistentSearchGeolocation())
    return;

  delegate_.reset(new SearchEngineDelegateImpl(profile_));
  delegate_->SetDSEChangedCallback(base::Bind(
      &SearchGeolocationService::OnDSEChanged, base::Unretained(this)));

  InitializeDSEGeolocationSettingIfNeeded();

  // Make sure the setting is valid now. It's possible that the setting has
  // become invalid either by changes being made to enterprise policy, or while
  // the flag to enable consistent search geolocation was off.
  EnsureDSEGeolocationSettingIsValid();
}

bool SearchGeolocationService::UseDSEGeolocationSetting(
    const url::Origin& requesting_origin) {
  if (!UseConsistentSearchGeolocation())
    return false;

  if (requesting_origin.scheme() != url::kHttpsScheme)
    return false;

  if (!requesting_origin.IsSameOriginWith(delegate_->GetDSEOrigin()))
    return false;

  // If the content setting for the DSE CCTLD is controlled by policy, and is st
  // to ASK, don't use the DSE geolocation setting.
  if (!IsContentSettingUserSettable() &&
      GetCurrentContentSetting() == CONTENT_SETTING_ASK) {
    return false;
  }

  return true;
}

bool SearchGeolocationService::GetDSEGeolocationSetting() {
  // Make sure the setting is valid, in case enterprise policy has changed.
  // TODO(benwells): Check if enterprise policy can change while Chrome is
  // running. If it can't this call is probably not needed.
  EnsureDSEGeolocationSettingIsValid();

  return GetDSEGeolocationPref().setting;
}

void SearchGeolocationService::SetDSEGeolocationSetting(bool setting) {
  PrefValue pref = GetDSEGeolocationPref();
  if (setting == pref.setting)
    return;

  // If the user cannot change their geolocation content setting (e.g. due to
  // enterprise policy), they also can't change this preference so just bail
  // out.
  if (!IsContentSettingUserSettable())
    return;

  pref.setting = setting;
  SetDSEGeolocationPref(pref);

  ResetContentSetting();
}

void SearchGeolocationService::Shutdown() {
  delegate_.reset();
}

SearchGeolocationService::~SearchGeolocationService() {}

void SearchGeolocationService::OnDSEChanged() {
  base::string16 new_dse_name = delegate_->GetDSEName();
  PrefValue pref = GetDSEGeolocationPref();
  ContentSetting content_setting = GetCurrentContentSetting();

  // Remove any geolocation embargo on the URL.
  PermissionDecisionAutoBlocker::GetForProfile(profile_)->RemoveEmbargoByUrl(
      delegate_->GetDSEOrigin().GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION);
  if (content_setting == CONTENT_SETTING_BLOCK && pref.setting) {
    pref.setting = false;
  } else if (content_setting == CONTENT_SETTING_ALLOW && !pref.setting) {
    ResetContentSetting();
  }

  if (new_dse_name != pref.dse_name && pref.setting)
    SearchGeolocationDisclosureTabHelper::ResetDisclosure(profile_);

  pref.dse_name = new_dse_name;
  SetDSEGeolocationPref(pref);
}

void SearchGeolocationService::InitializeDSEGeolocationSettingIfNeeded() {
  // Migrate the pref if it hasn't been migrated yet.
  if (pref_service_->HasPrefPath(
          prefs::kGoogleDSEGeolocationSettingDeprecated)) {
    const base::DictionaryValue* dict = pref_service_->GetDictionary(
        prefs::kGoogleDSEGeolocationSettingDeprecated);

    bool setting = false;
    dict->GetBoolean(kDSESettingKey, &setting);
    bool was_google = false;
    dict->GetBoolean(kIsGoogleSearchEngineKey, &was_google);

    // Make sure the new setting is configured to match the old one.
    PrefValue pref;
    pref.dse_name = delegate_->GetDSEName();
    pref.setting = setting;
    SetDSEGeolocationPref(pref);

    // If the setting isn't for Google, reset the disclosure so it will be
    // shown, even if it was shown previously for Google.
    if (!was_google && setting)
      SearchGeolocationDisclosureTabHelper::ResetDisclosure(profile_);

    // Now remove the old setting so there is no more migration.
    pref_service_->ClearPref(prefs::kGoogleDSEGeolocationSettingDeprecated);
    return;
  }

  // Initialize the pref if it hasn't been initialized yet.
  if (!pref_service_->HasPrefPath(prefs::kDSEGeolocationSetting)) {
    ContentSetting content_setting = GetCurrentContentSetting();

    PrefValue pref;
    pref.dse_name = delegate_->GetDSEName();
    pref.setting = content_setting != CONTENT_SETTING_BLOCK;
    SetDSEGeolocationPref(pref);

    SearchGeolocationDisclosureTabHelper::ResetDisclosure(profile_);
  }
}

void SearchGeolocationService::EnsureDSEGeolocationSettingIsValid() {
  PrefValue pref = GetDSEGeolocationPref();
  ContentSetting content_setting = GetCurrentContentSetting();
  bool new_setting = pref.setting;

  if (pref.setting && content_setting == CONTENT_SETTING_BLOCK) {
    new_setting = false;
  } else if (!pref.setting && content_setting == CONTENT_SETTING_ALLOW) {
    new_setting = true;
  }

  if (pref.setting != new_setting) {
    pref.setting = new_setting;
    SetDSEGeolocationPref(pref);
  }
}

SearchGeolocationService::PrefValue
SearchGeolocationService::GetDSEGeolocationPref() {
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(prefs::kDSEGeolocationSetting);

  PrefValue pref;
  base::string16 dse_name;
  bool setting;
  if (dict->GetString(kDSENameKey, &dse_name) &&
      dict->GetBoolean(kDSESettingKey, &setting)) {
    pref.dse_name = dse_name;
    pref.setting = setting;
  }

  return pref;
}

void SearchGeolocationService::SetDSEGeolocationPref(
    const SearchGeolocationService::PrefValue& pref) {
  base::DictionaryValue dict;
  dict.SetString(kDSENameKey, pref.dse_name);
  dict.SetBoolean(kDSESettingKey, pref.setting);
  pref_service_->Set(prefs::kDSEGeolocationSetting, dict);
}

ContentSetting SearchGeolocationService::GetCurrentContentSetting() {
  url::Origin origin = delegate_->GetDSEOrigin();
  return host_content_settings_map_->GetContentSetting(
      origin.GetURL(), origin.GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string());
}

void SearchGeolocationService::ResetContentSetting() {
  url::Origin origin = delegate_->GetDSEOrigin();
  host_content_settings_map_->SetContentSettingDefaultScope(
      origin.GetURL(), origin.GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(), CONTENT_SETTING_DEFAULT);
}

bool SearchGeolocationService::IsContentSettingUserSettable() {
  content_settings::SettingInfo info;
  url::Origin origin = delegate_->GetDSEOrigin();
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          origin.GetURL(), origin.GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string(), &info);
  return info.source == content_settings::SETTING_SOURCE_USER;
}

bool SearchGeolocationService::UseConsistentSearchGeolocation() {
  return base::FeatureList::IsEnabled(features::kConsistentOmniboxGeolocation);
}

void SearchGeolocationService::SetSearchEngineDelegateForTest(
    std::unique_ptr<SearchEngineDelegate> delegate) {
  delegate_ = std::move(delegate);
  delegate_->SetDSEChangedCallback(base::Bind(
      &SearchGeolocationService::OnDSEChanged, base::Unretained(this)));
}
