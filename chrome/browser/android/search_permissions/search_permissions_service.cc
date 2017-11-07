// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
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

const char kDSESettingKey[] = "dse_setting";
const char kDSENameKey[] = "dse_name";

// Default implementation of SearchEngineDelegate that is used for production
// code.
class SearchEngineDelegateImpl
    : public SearchPermissionsService::SearchEngineDelegate,
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
        return url::Origin::Create(search_url);
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

struct SearchPermissionsService::PrefValue {
  base::string16 dse_name;
  bool setting = false;
};

// static
SearchPermissionsService*
SearchPermissionsService::Factory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SearchPermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SearchPermissionsService::Factory*
SearchPermissionsService::Factory::GetInstance() {
  return base::Singleton<SearchPermissionsService::Factory>::get();
}

SearchPermissionsService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "SearchPermissionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchPermissionsService::Factory::~Factory() {}

bool SearchPermissionsService::Factory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* SearchPermissionsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SearchPermissionsService(Profile::FromBrowserContext(context));
}

void SearchPermissionsService::Factory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDSEGeolocationSetting);
}

SearchPermissionsService::SearchPermissionsService(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile_)) {
  // This class should never be constructed in incognito.
  DCHECK(!profile_->IsOffTheRecord());

  delegate_.reset(new SearchEngineDelegateImpl(profile_));
  delegate_->SetDSEChangedCallback(base::Bind(
      &SearchPermissionsService::OnDSEChanged, base::Unretained(this)));

  InitializeDSEGeolocationSettingIfNeeded();

  // Make sure the setting is valid now. It's possible that the setting has
  // become invalid either by changes being made to enterprise policy, or while
  // the flag to enable consistent search geolocation was off.
  EnsureDSEGeolocationSettingIsValid();
}

bool SearchPermissionsService::UseDSEGeolocationSetting(
    const url::Origin& requesting_origin) {
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

bool SearchPermissionsService::GetDSEGeolocationSetting() {
  // Make sure the setting is valid, in case enterprise policy has changed.
  // TODO(benwells): Check if enterprise policy can change while Chrome is
  // running. If it can't this call is probably not needed.
  EnsureDSEGeolocationSettingIsValid();

  return GetDSEGeolocationPref().setting;
}

void SearchPermissionsService::SetDSEGeolocationSetting(bool setting) {
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

url::Origin SearchPermissionsService::GetDSEOriginIfEnabled() {
  url::Origin dse_origin = delegate_->GetDSEOrigin();
  if (UseDSEGeolocationSetting(dse_origin))
    return dse_origin;
  return url::Origin();
}

void SearchPermissionsService::Shutdown() {
  delegate_.reset();
}

SearchPermissionsService::~SearchPermissionsService() {}

void SearchPermissionsService::OnDSEChanged() {
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

void SearchPermissionsService::InitializeDSEGeolocationSettingIfNeeded() {
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

void SearchPermissionsService::EnsureDSEGeolocationSettingIsValid() {
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

SearchPermissionsService::PrefValue
SearchPermissionsService::GetDSEGeolocationPref() {
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

void SearchPermissionsService::SetDSEGeolocationPref(
    const SearchPermissionsService::PrefValue& pref) {
  base::DictionaryValue dict;
  dict.SetString(kDSENameKey, pref.dse_name);
  dict.SetBoolean(kDSESettingKey, pref.setting);
  pref_service_->Set(prefs::kDSEGeolocationSetting, dict);
}

ContentSetting SearchPermissionsService::GetCurrentContentSetting() {
  url::Origin origin = delegate_->GetDSEOrigin();
  return host_content_settings_map_->GetContentSetting(
      origin.GetURL(), origin.GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string());
}

void SearchPermissionsService::ResetContentSetting() {
  url::Origin origin = delegate_->GetDSEOrigin();
  host_content_settings_map_->SetContentSettingDefaultScope(
      origin.GetURL(), origin.GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(), CONTENT_SETTING_DEFAULT);
}

bool SearchPermissionsService::IsContentSettingUserSettable() {
  content_settings::SettingInfo info;
  url::Origin origin = delegate_->GetDSEOrigin();
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          origin.GetURL(), origin.GetURL(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string(), &info);
  return info.source == content_settings::SETTING_SOURCE_USER;
}

void SearchPermissionsService::SetSearchEngineDelegateForTest(
    std::unique_ptr<SearchEngineDelegate> delegate) {
  delegate_ = std::move(delegate);
  delegate_->SetDSEChangedCallback(base::Bind(
      &SearchPermissionsService::OnDSEChanged, base::Unretained(this)));
}
