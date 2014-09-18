// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/constants.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "url/gurl.h"

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

bool IsValidSetting(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY ||
          setting == CONTENT_SETTING_BLOCK);
}

bool IsAllowed(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY);
}

}  // namespace

// static
scoped_refptr<CookieSettings> CookieSettings::Factory::GetForProfile(
    Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return static_cast<CookieSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
CookieSettings::Factory* CookieSettings::Factory::GetInstance() {
  return Singleton<CookieSettings::Factory>::get();
}

CookieSettings::Factory::Factory()
    : RefcountedBrowserContextKeyedServiceFactory(
        "CookieSettings",
        BrowserContextDependencyManager::GetInstance()) {
}

CookieSettings::Factory::~Factory() {}

void CookieSettings::Factory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kBlockThirdPartyCookies,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

content::BrowserContext* CookieSettings::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

scoped_refptr<RefcountedBrowserContextKeyedService>
CookieSettings::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new CookieSettings(profile->GetHostContentSettingsMap(),
                            profile->GetPrefs());
}

CookieSettings::CookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs)
    : host_content_settings_map_(host_content_settings_map),
      block_third_party_cookies_(
          prefs->GetBoolean(prefs::kBlockThirdPartyCookies)) {
  if (block_third_party_cookies_) {
    content::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingEnabled"));
  } else {
    content::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingDisabled"));
  }

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&CookieSettings::OnBlockThirdPartyCookiesChanged,
                 base::Unretained(this)));
}

ContentSetting
CookieSettings::GetDefaultCookieSetting(std::string* provider_id) const {
  return host_content_settings_map_->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, provider_id);
}

bool CookieSettings::IsReadingCookieAllowed(const GURL& url,
                                            const GURL& first_party_url) const {
  ContentSetting setting = GetCookieSetting(url, first_party_url, false, NULL);
  return IsAllowed(setting);
}

bool CookieSettings::IsSettingCookieAllowed(const GURL& url,
                                            const GURL& first_party_url) const {
  ContentSetting setting = GetCookieSetting(url, first_party_url, true, NULL);
  return IsAllowed(setting);
}

bool CookieSettings::IsCookieSessionOnly(const GURL& origin) const {
  ContentSetting setting = GetCookieSetting(origin, origin, true, NULL);
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_SESSION_ONLY);
}

void CookieSettings::GetCookieSettings(
    ContentSettingsForOneType* settings) const {
  return host_content_settings_map_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), settings);
}

void CookieSettings::SetDefaultCookieSetting(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, setting);
}

void CookieSettings::SetCookieSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  if (setting == CONTENT_SETTING_SESSION_ONLY) {
    DCHECK(secondary_pattern == ContentSettingsPattern::Wildcard());
  }
  host_content_settings_map_->SetContentSetting(primary_pattern,
                                                secondary_pattern,
                                                CONTENT_SETTINGS_TYPE_COOKIES,
                                                std::string(),
                                                setting);
}

void CookieSettings::ResetCookieSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  host_content_settings_map_->SetContentSetting(primary_pattern,
                                                secondary_pattern,
                                                CONTENT_SETTINGS_TYPE_COOKIES,
                                                std::string(),
                                                CONTENT_SETTING_DEFAULT);
}

void CookieSettings::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_.RemoveAll();
}

ContentSetting CookieSettings::GetCookieSetting(
    const GURL& url,
    const GURL& first_party_url,
    bool setting_cookie,
    content_settings::SettingSource* source) const {
  if (HostContentSettingsMap::ShouldAllowAllContent(
        url, first_party_url, CONTENT_SETTINGS_TYPE_COOKIES))
    return CONTENT_SETTING_ALLOW;

  // First get any host-specific settings.
  content_settings::SettingInfo info;
  scoped_ptr<base::Value> value = host_content_settings_map_->GetWebsiteSetting(
      url,
      first_party_url,
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(),
      &info);
  if (source)
    *source = info.source;

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply that rule.
  if (info.primary_pattern.MatchesAllHosts() &&
      info.secondary_pattern.MatchesAllHosts() &&
      ShouldBlockThirdPartyCookies() &&
      !first_party_url.SchemeIs(extensions::kExtensionScheme)) {
    net::StaticCookiePolicy policy(
        net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);
    int rv;
    if (setting_cookie)
      rv = policy.CanSetCookie(url, first_party_url);
    else
      rv = policy.CanGetCookies(url, first_party_url);
    DCHECK_NE(net::ERR_IO_PENDING, rv);
    if (rv != net::OK)
      return CONTENT_SETTING_BLOCK;
  }

  // We should always have a value, at least from the default provider.
  DCHECK(value.get());
  return content_settings::ValueToContentSetting(value.get());
}

CookieSettings::~CookieSettings() {}

void CookieSettings::OnBlockThirdPartyCookiesChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock auto_lock(lock_);
  block_third_party_cookies_ = pref_change_registrar_.prefs()->GetBoolean(
      prefs::kBlockThirdPartyCookies);
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}
