// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/content_settings_policy_provider.h"
#include "chrome/browser/content_settings/content_settings_pref_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/base/static_cookie_policy.h"

namespace {

// Returns true if we should allow all content types for this URL.  This is
// true for various internal objects like chrome:// URLs, so UI and other
// things users think of as "not webpages" don't break.
static bool ShouldAllowAllContent(const GURL& url) {
  return url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme) ||
         url.SchemeIs(chrome::kUserScriptScheme);
}

typedef linked_ptr<content_settings::DefaultProviderInterface>
    DefaultContentSettingsProviderPtr;
typedef std::vector<DefaultContentSettingsProviderPtr>::iterator
    DefaultProviderIterator;
typedef std::vector<DefaultContentSettingsProviderPtr>::const_iterator
    ConstDefaultProviderIterator;

typedef linked_ptr<content_settings::ProviderInterface> ProviderPtr;
typedef std::vector<ProviderPtr>::iterator ProviderIterator;
typedef std::vector<ProviderPtr>::const_iterator ConstProviderIterator;

typedef content_settings::ProviderInterface::Rules Rules;
typedef content_settings::ProviderInterface::Rules::iterator
    rules_iterator;
typedef content_settings::ProviderInterface::Rules::const_iterator
    const_rules_iterator;

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile),
      is_off_the_record_(profile_->IsOffTheRecord()),
      updating_preferences_(false),
      block_third_party_cookies_(false),
      is_block_third_party_cookies_managed_(false) {
  // The order in which the default content settings providers are created is
  // critical, as providers that are further down in the list (i.e. added later)
  // override providers further up.
  default_content_settings_providers_.push_back(
      DefaultContentSettingsProviderPtr(
          new content_settings::PrefDefaultProvider(profile)));
  default_content_settings_providers_.push_back(
      DefaultContentSettingsProviderPtr(
          new content_settings::PolicyDefaultProvider(profile)));

  PrefService* prefs = profile_->GetPrefs();

  // TODO(markusheintz): Discuss whether it is sensible to move migration code
  // to PrefContentSettingsProvider.
  MigrateObsoleteCookiePref(prefs);

  // Read misc. global settings.
  block_third_party_cookies_ =
      prefs->GetBoolean(prefs::kBlockThirdPartyCookies);
  if (block_third_party_cookies_) {
    UserMetrics::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingEnabled"));
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("ThirdPartyCookieBlockingDisabled"));
  }
  is_block_third_party_cookies_managed_ =
      prefs->IsManagedPreference(prefs::kBlockThirdPartyCookies);
  block_nonsandboxed_plugins_ =
      prefs->GetBoolean(prefs::kBlockNonsandboxedPlugins);

  // User defined non default content settings are provided by the PrefProvider.
  // The order in which the content settings providers are created is critical,
  // as providers that are further up in the list (i.e. added earlier) override
  // providers further down.
  content_settings_providers_.push_back(
      make_linked_ptr(new content_settings::PolicyProvider(profile)));
  content_settings_providers_.push_back(
      make_linked_ptr(new content_settings::PrefProvider(profile)));

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kBlockThirdPartyCookies, this);
  pref_change_registrar_.Add(prefs::kBlockNonsandboxedPlugins, this);
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBlockThirdPartyCookies, false);
  prefs->RegisterBooleanPref(prefs::kBlockNonsandboxedPlugins, false);
  prefs->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex, 0);

  // Obsolete prefs, for migration:
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::StaticCookiePolicy::ALLOW_ALL_COOKIES);

  // Register the prefs for the content settings providers.
  content_settings::PrefDefaultProvider::RegisterUserPrefs(prefs);
  content_settings::PolicyDefaultProvider::RegisterUserPrefs(prefs);
  content_settings::PrefProvider::RegisterUserPrefs(prefs);
  content_settings::PolicyProvider::RegisterUserPrefs(prefs);
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type) const {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  for (ConstDefaultProviderIterator provider =
           default_content_settings_providers_.begin();
       provider != default_content_settings_providers_.end(); ++provider) {
    ContentSetting provided_setting =
        (*provider)->ProvideDefaultSetting(content_type);
    if (provided_setting != CONTENT_SETTING_DEFAULT)
      setting = provided_setting;
  }
  // The method GetDefaultContentSetting always has to return an explicit
  // value that is to be used as default. We here rely on the
  // PrefContentSettingProvider to always provide a value.
  CHECK_NE(CONTENT_SETTING_DEFAULT, setting);
  return setting;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  ContentSetting setting = GetNonDefaultContentSetting(url,
                                                       content_type,
                                                       resource_identifier);
  if (setting == CONTENT_SETTING_DEFAULT)
    return GetDefaultContentSetting(content_type);
  return setting;
}

ContentSetting HostContentSettingsMap::GetNonDefaultContentSetting(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier) const {
  if (ShouldAllowAllContent(url))
    return CONTENT_SETTING_ALLOW;

  // Iterate through the list of providers and break when the first non default
  // setting is found.
  ContentSetting provided_setting(CONTENT_SETTING_DEFAULT);
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    provided_setting = (*provider)->GetContentSetting(
        url, url, content_type, resource_identifier);
    bool isManaged = (*provider)->ContentSettingsTypeIsManaged(content_type);
    if (provided_setting != CONTENT_SETTING_DEFAULT || isManaged)
      return provided_setting;
  }
  return provided_setting;
}

ContentSettings HostContentSettingsMap::GetContentSettings(
    const GURL& url) const {
  ContentSettings output = GetNonDefaultContentSettings(url);

  // If we require a resource identifier, set the content settings to default,
  // otherwise make the defaults explicit.
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    // A managed default content setting has the highest priority and hence
    // will overwrite any previously set value.
    if (output.settings[j] == CONTENT_SETTING_DEFAULT &&
        j != CONTENT_SETTINGS_TYPE_PLUGINS) {
      output.settings[j] = GetDefaultContentSetting(ContentSettingsType(j));
    }
  }
  return output;
}

ContentSettings HostContentSettingsMap::GetNonDefaultContentSettings(
    const GURL& url) const {
  if (ShouldAllowAllContent(url))
      return ContentSettings(CONTENT_SETTING_ALLOW);

  ContentSettings output(CONTENT_SETTING_DEFAULT);
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
    output.settings[j] = GetNonDefaultContentSetting(
        url, ContentSettingsType(j) , "");
  }
  return output;
}

void HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    SettingsForOneType* settings) const {
  DCHECK(settings);
  settings->clear();

  // Collect content_settings::Rules for the given content_type and
  // resource_identifier from the content settings providers.
  Rules content_settings_rules;
  for (ConstProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    // TODO(markusheintz): Only the rules that are applied should be collected.
    // Merge rules.
    // TODO(markusheintz): GetAllContentSettingsRules should maybe not clear the
    // passed vector in case rule sets are just unified.
    Rules rules;
    (*provider)->GetAllContentSettingsRules(
        content_type, resource_identifier, &rules);
    content_settings_rules.insert(content_settings_rules.end(),
                                  rules.begin(),
                                  rules.end());
  }

  // convert Rules to SettingsForOneType
  for (const_rules_iterator rule_iterator =
           content_settings_rules.begin();
       rule_iterator != content_settings_rules.end();
       ++rule_iterator) {
    settings->push_back(std::make_pair(ContentSettingsPattern(
        rule_iterator->requesting_url_pattern),
        rule_iterator->content_setting));
  }
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  for (DefaultProviderIterator provider =
           default_content_settings_providers_.begin();
       provider != default_content_settings_providers_.end(); ++provider) {
    (*provider)->UpdateDefaultSetting(content_type, setting);
  }
}

void HostContentSettingsMap::SetContentSetting(
    const ContentSettingsPattern& pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    (*provider)->SetContentSetting(
        pattern, pattern, content_type, resource_identifier, setting);
  }
}

void HostContentSettingsMap::AddExceptionForURL(
    const GURL& url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    ContentSetting setting) {
  // Make sure there is no entry that would override the pattern we are about
  // to insert for exactly this URL.
  SetContentSetting(ContentSettingsPattern::FromURLNoWildcard(url),
                    content_type,
                    resource_identifier,
                    CONTENT_SETTING_DEFAULT);
  SetContentSetting(ContentSettingsPattern::FromURL(url),
                    content_type,
                    resource_identifier,
                    setting);
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  for (ProviderIterator provider = content_settings_providers_.begin();
       provider != content_settings_providers_.end();
       ++provider) {
    (*provider)->ClearAllContentSettingsRules(content_type);
  }
}

void HostContentSettingsMap::SetBlockThirdPartyCookies(bool block) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This setting may not be directly modified for OTR sessions.  Instead, it
  // is synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  // If the preference block-third-party-cookies is managed then do not allow to
  // change it.
  if (prefs->IsManagedPreference(prefs::kBlockThirdPartyCookies)) {
    NOTREACHED();
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    block_third_party_cookies_ = block;
  }

  if (block)
    prefs->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  else
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
}

void HostContentSettingsMap::SetBlockNonsandboxedPlugins(bool block) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This setting may not be directly modified for OTR sessions.  Instead, it
  // is synced to the main profile's setting.
  if (is_off_the_record_) {
    NOTREACHED();
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    block_nonsandboxed_plugins_ = block;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (block) {
    UserMetrics::RecordAction(
        UserMetricsAction("BlockNonsandboxedPlugins_Enable"));
    prefs->SetBoolean(prefs::kBlockNonsandboxedPlugins, true);
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("BlockNonsandboxedPlugins_Disable"));
    prefs->ClearPref(prefs::kBlockNonsandboxedPlugins);
  }
}

void HostContentSettingsMap::ResetToDefaults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);
    for (DefaultProviderIterator provider =
             default_content_settings_providers_.begin();
         provider != default_content_settings_providers_.end(); ++provider) {
      (*provider)->ResetToDefaults();
    }

    for (ProviderIterator provider = content_settings_providers_.begin();
         provider != content_settings_providers_.end();
         ++provider) {
      (*provider)->ResetToDefaults();
    }

    // Don't reset block third party cookies if they are managed.
    if (!IsBlockThirdPartyCookiesManaged())
      block_third_party_cookies_ = false;
    block_nonsandboxed_plugins_ = false;
  }

  if (!is_off_the_record_) {
    PrefService* prefs = profile_->GetPrefs();
    updating_preferences_ = true;
    // If the block third party cookies preference is managed we still must
    // clear it in order to restore the default value for later when the
    // preference is not managed anymore.
    prefs->ClearPref(prefs::kBlockThirdPartyCookies);
    prefs->ClearPref(prefs::kBlockNonsandboxedPlugins);
    updating_preferences_ = false;
    NotifyObservers(ContentSettingsDetails(ContentSettingsPattern(),
                                           CONTENT_SETTINGS_TYPE_DEFAULT,
                                           ""));
  }
}

void HostContentSettingsMap::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(profile_->GetPrefs(), Source<PrefService>(source).ptr());
    if (updating_preferences_)
      return;

    std::string* name = Details<std::string>(details).ptr();
    if (*name == prefs::kBlockThirdPartyCookies) {
      base::AutoLock auto_lock(lock_);
      block_third_party_cookies_ = profile_->GetPrefs()->GetBoolean(
          prefs::kBlockThirdPartyCookies);
      is_block_third_party_cookies_managed_ =
          profile_->GetPrefs()->IsManagedPreference(
              prefs::kBlockThirdPartyCookies);
    } else if (*name == prefs::kBlockNonsandboxedPlugins) {
      base::AutoLock auto_lock(lock_);
      block_nonsandboxed_plugins_ = profile_->GetPrefs()->GetBoolean(
          prefs::kBlockNonsandboxedPlugins);
    } else {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    if (!is_off_the_record_) {
      NotifyObservers(ContentSettingsDetails(ContentSettingsPattern(),
                                             CONTENT_SETTINGS_TYPE_DEFAULT,
                                             ""));
    }
  } else if (type == NotificationType::PROFILE_DESTROYED) {
    DCHECK_EQ(profile_, Source<Profile>(source).ptr());
    UnregisterObservers();
  } else {
    NOTREACHED() << "Unexpected notification";
  }
}

HostContentSettingsMap::~HostContentSettingsMap() {
  UnregisterObservers();
}

bool HostContentSettingsMap::IsDefaultContentSettingManaged(
    ContentSettingsType content_type) const {
  for (ConstDefaultProviderIterator provider =
           default_content_settings_providers_.begin();
       provider != default_content_settings_providers_.end(); ++provider) {
    if ((*provider)->DefaultSettingIsManaged(content_type))
      return true;
  }
  return false;
}

void HostContentSettingsMap::NotifyObservers(
    const ContentSettingsDetails& details) {
  NotificationService::current()->Notify(
      NotificationType::CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(this),
      Details<const ContentSettingsDetails>(&details));
}

void HostContentSettingsMap::UnregisterObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;
  pref_change_registrar_.RemoveAll();
  notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                 Source<Profile>(profile_));
  profile_ = NULL;
}

void HostContentSettingsMap::MigrateObsoleteCookiePref(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kCookieBehavior)) {
    int cookie_behavior = prefs->GetInteger(prefs::kCookieBehavior);
    prefs->ClearPref(prefs::kCookieBehavior);
    if (!prefs->HasPrefPath(prefs::kDefaultContentSettings)) {
        SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
            (cookie_behavior == net::StaticCookiePolicy::BLOCK_ALL_COOKIES) ?
                CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
    }
    if (!prefs->HasPrefPath(prefs::kBlockThirdPartyCookies)) {
      SetBlockThirdPartyCookies(cookie_behavior ==
          net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);
    }
  }
}
