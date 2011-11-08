// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings.h"

#include "base/command_line.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

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

// |ProfileKeyedFactory| is the owner of the |ProfileKeyedService|s. This
// wrapper class allows others to hold shared pointers to CookieSettings.
class CookieSettingsWrapper : public ProfileKeyedService {
 public:
  explicit CookieSettingsWrapper(scoped_refptr<CookieSettings> cookie_settings)
      : cookie_settings_(cookie_settings) {}
  virtual ~CookieSettingsWrapper() {}

  CookieSettings* cookie_settings() { return cookie_settings_.get(); }

 private:
  // ProfileKeyedService methods:
  virtual void Shutdown() OVERRIDE {
    cookie_settings_->ShutdownOnUIThread();
  }

  scoped_refptr<CookieSettings> cookie_settings_;
};

// static
CookieSettings::Factory* CookieSettings::Factory::GetInstance() {
  return Singleton<CookieSettings::Factory>::get();
}

CookieSettingsWrapper* CookieSettings::Factory::GetWrapperForProfile(
    Profile* profile) {
  return static_cast<CookieSettingsWrapper*>(
      GetServiceForProfile(profile, true));
}

CookieSettings::Factory::Factory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

ProfileKeyedService* CookieSettings::Factory::BuildServiceInstanceFor(
      Profile* profile) const {
  scoped_refptr<CookieSettings> cookie_settings = new CookieSettings(
      profile->GetHostContentSettingsMap(),
      profile->GetPrefs());
  return new CookieSettingsWrapper(cookie_settings);
}

CookieSettings::CookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs)
    : host_content_settings_map_(host_content_settings_map),
      block_third_party_cookies_(
          prefs->GetBoolean(prefs::kBlockThirdPartyCookies)) {
  if (block_third_party_cookies_) {
    UserMetrics::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingEnabled"));
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingDisabled"));
  }

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kBlockThirdPartyCookies, this);
}

CookieSettings::~CookieSettings() {
}

// static
CookieSettings* CookieSettings::GetForProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CookieSettings* cookie_settings =
      Factory::GetInstance()->GetWrapperForProfile(profile)->cookie_settings();
  DCHECK(cookie_settings);
  return cookie_settings;
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
      CONTENT_SETTINGS_TYPE_COOKIES, "", settings);
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
  host_content_settings_map_->SetContentSetting(
      primary_pattern, secondary_pattern, CONTENT_SETTINGS_TYPE_COOKIES, "",
      setting);
}

void CookieSettings::ResetCookieSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  host_content_settings_map_->SetContentSetting(
      primary_pattern, secondary_pattern, CONTENT_SETTINGS_TYPE_COOKIES, "",
      CONTENT_SETTING_DEFAULT);
}

void CookieSettings::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    PrefService* prefs = content::Source<PrefService>(source).ptr();
    std::string* name = content::Details<std::string>(details).ptr();
    if (*name == prefs::kBlockThirdPartyCookies) {
      base::AutoLock auto_lock(lock_);
      block_third_party_cookies_ = prefs->GetBoolean(
          prefs::kBlockThirdPartyCookies);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }
  } else {
    NOTREACHED() << "Unexpected notification";
  }
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
  scoped_ptr<base::Value> value(
      host_content_settings_map_->GetWebsiteSetting(
          url, first_party_url, CONTENT_SETTINGS_TYPE_COOKIES, "", &info));
  if (source)
    *source = info.source;

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply that rule.
  if (info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard() &&
      ShouldBlockThirdPartyCookies() &&
      !first_party_url.SchemeIs(chrome::kExtensionScheme)) {
    bool not_strict = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kOnlyBlockSettingThirdPartyCookies);
    net::StaticCookiePolicy policy(not_strict ?
        net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES :
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

// static
void CookieSettings::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBlockThirdPartyCookies,
                             false,
                             PrefService::SYNCABLE_PREF);
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}
