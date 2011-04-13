// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/content_settings/content_settings_notification_provider.h"

#include "base/string_util.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"

namespace {

const ContentSetting kDefaultSetting = CONTENT_SETTING_ASK;

}  // namespace

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// NotificationProvider
//

// static
void NotificationProvider::RegisterUserPrefs(PrefService* user_prefs) {
  if (!user_prefs->FindPreference(prefs::kDesktopNotificationAllowedOrigins))
    user_prefs->RegisterListPref(prefs::kDesktopNotificationAllowedOrigins);
  if (!user_prefs->FindPreference(prefs::kDesktopNotificationDeniedOrigins))
    user_prefs->RegisterListPref(prefs::kDesktopNotificationDeniedOrigins);
}

// TODO(markusheintz): Re-factoring in progress. Do not move or touch the
// following two static methods as you might cause trouble. Thanks!

// static
ContentSettingsPattern NotificationProvider::ToContentSettingsPattern(
    const GURL& origin) {
  // Fix empty GURLs.
  if (origin.spec().empty()) {
    std::string pattern_spec(chrome::kFileScheme);
    pattern_spec += chrome::kStandardSchemeSeparator;
    return ContentSettingsPattern(pattern_spec);
  }
  return ContentSettingsPattern::FromURLNoWildcard(origin);
}

// static
GURL NotificationProvider::ToGURL(const ContentSettingsPattern& pattern) {
  std::string pattern_spec(pattern.AsString());

  if (pattern_spec.empty() ||
      StartsWithASCII(pattern_spec,
                      std::string(ContentSettingsPattern::kDomainWildcard),
                      true)) {
    NOTREACHED();
  }

  std::string url_spec("");
  if (StartsWithASCII(pattern_spec, std::string(chrome::kFileScheme), false)) {
    url_spec += pattern_spec;
  } else if (!pattern.scheme().empty()) {
    url_spec += pattern.scheme();
    url_spec += chrome::kStandardSchemeSeparator;
    url_spec += pattern_spec;
  }

  return GURL(url_spec);
}

NotificationProvider::NotificationProvider(
    Profile* profile)
    : profile_(profile) {
  prefs_registrar_.Init(profile_->GetPrefs());
  StartObserving();
}

NotificationProvider::~NotificationProvider() {
  StopObserving();
}

bool NotificationProvider::ContentSettingsTypeIsManaged(
      ContentSettingsType content_type) {
  return false;
}

ContentSetting NotificationProvider::GetContentSetting(
      const GURL& requesting_url,
      const GURL& embedding_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const {
  if (content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return CONTENT_SETTING_DEFAULT;

  return GetContentSetting(requesting_url);
}

void NotificationProvider::SetContentSetting(
      const ContentSettingsPattern& requesting_url_pattern,
      const ContentSettingsPattern& embedding_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting) {
  if (content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return;

  GURL origin = ToGURL(requesting_url_pattern);
  if (CONTENT_SETTING_ALLOW == content_setting) {
    GrantPermission(origin);
  } else if (CONTENT_SETTING_BLOCK == content_setting) {
    DenyPermission(origin);
  } else if (CONTENT_SETTING_DEFAULT == content_setting) {
    ContentSetting current_setting = GetContentSetting(origin);
    if (CONTENT_SETTING_ALLOW == current_setting) {
      ResetAllowedOrigin(origin);
    } else if (CONTENT_SETTING_BLOCK == current_setting) {
      ResetBlockedOrigin(origin);
    } else {
      NOTREACHED();
    }
  } else {
      NOTREACHED();
  }
}

void NotificationProvider::GetAllContentSettingsRules(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Rules* content_setting_rules) const {
  if (content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return;

  std::vector<GURL> allowed_origins = GetAllowedOrigins();
  std::vector<GURL> denied_origins = GetBlockedOrigins();

  for (std::vector<GURL>::iterator url = allowed_origins.begin();
       url != allowed_origins.end();
       ++url) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromURLNoWildcard(*url);
    content_setting_rules->push_back(Rule(
        pattern,
        pattern,
        CONTENT_SETTING_ALLOW));
  }
  for (std::vector<GURL>::iterator url = denied_origins.begin();
       url != denied_origins.end();
       ++url) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromURLNoWildcard(*url);
    content_setting_rules->push_back(Rule(
        pattern,
        pattern,
        CONTENT_SETTING_BLOCK));
  }
}

void NotificationProvider::ClearAllContentSettingsRules(
      ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    ResetAllOrigins();
}

void NotificationProvider::ResetToDefaults() {
  ResetAllOrigins();
}

void NotificationProvider::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    const std::string& name = *Details<std::string>(details).ptr();
    OnPrefsChanged(name);
  } else if (NotificationType::PROFILE_DESTROYED == type) {
    StopObserving();
  }
}

/////////////////////////////////////////////////////////////////////
// Private
//

void NotificationProvider::StartObserving() {
  if (!profile_->IsOffTheRecord()) {
    prefs_registrar_.Add(prefs::kDesktopNotificationDefaultContentSetting,
                         this);
    prefs_registrar_.Add(prefs::kDesktopNotificationAllowedOrigins, this);
    prefs_registrar_.Add(prefs::kDesktopNotificationDeniedOrigins, this);

    notification_registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                                NotificationService::AllSources());
  }

  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

void NotificationProvider::StopObserving() {
  if (!profile_->IsOffTheRecord()) {
    prefs_registrar_.RemoveAll();
  }
  notification_registrar_.RemoveAll();
}

void NotificationProvider::OnPrefsChanged(const std::string& pref_name) {
  if (pref_name == prefs::kDesktopNotificationAllowedOrigins) {
    NotifySettingsChange();
  } else if (pref_name == prefs::kDesktopNotificationDeniedOrigins) {
    NotifySettingsChange();
  }
}

void NotificationProvider::NotifySettingsChange() {
  // TODO(markusheintz): Re-factoring work in progress: Replace the
  // DESKTOP_NOTIFICATION_SETTINGS_CHANGED with a CONTENT_SETTINGS_CHANGED
  // notification, and use the HostContentSettingsMap as source once this
  // content settings provider in integrated in the HostContentSetttingsMap.
  NotificationService::current()->Notify(
      NotificationType::DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
      Source<DesktopNotificationService>(
          DesktopNotificationServiceFactory::GetForProfile(profile_)),
      NotificationService::NoDetails());
}

std::vector<GURL> NotificationProvider::GetAllowedOrigins() const {
  std::vector<GURL> allowed_origins;
  PrefService* prefs = profile_->GetPrefs();
  const ListValue* allowed_sites =
      prefs->GetList(prefs::kDesktopNotificationAllowedOrigins);
  if (allowed_sites) {
    // TODO(markusheintz): Remove dependency to PrefsCache
    NotificationsPrefsCache::ListValueToGurlVector(*allowed_sites,
                                                   &allowed_origins);
  }
  return allowed_origins;
}

std::vector<GURL> NotificationProvider::GetBlockedOrigins() const {
  std::vector<GURL> denied_origins;
  PrefService* prefs = profile_->GetPrefs();
  const ListValue* denied_sites =
      prefs->GetList(prefs::kDesktopNotificationDeniedOrigins);
  if (denied_sites) {
     // TODO(markusheintz): Remove dependency to PrefsCache
    NotificationsPrefsCache::ListValueToGurlVector(*denied_sites,
                                                   &denied_origins);
  }
  return denied_origins;
}

void NotificationProvider::GrantPermission(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PersistPermissionChange(origin, true);
  NotifySettingsChange();
}

void NotificationProvider::DenyPermission(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PersistPermissionChange(origin, false);
  NotifySettingsChange();
}

void NotificationProvider::PersistPermissionChange(
    const GURL& origin, bool is_allowed) {
  // Don't persist changes when incognito.
  if (profile_->IsOffTheRecord())
    return;
  PrefService* prefs = profile_->GetPrefs();

  // |Observe()| updates the whole permission set in the cache, but only a
  // single origin has changed. Hence, callers of this method manually
  // schedule a task to update the prefs cache, and the prefs observer is
  // disabled while the update runs.
  StopObserving();

  bool allowed_changed = false;
  bool denied_changed = false;

  {
    ListPrefUpdate update_allowed_sites(
        prefs, prefs::kDesktopNotificationAllowedOrigins);
    ListPrefUpdate update_denied_sites(
        prefs, prefs::kDesktopNotificationDeniedOrigins);
    ListValue* allowed_sites = update_allowed_sites.Get();
    ListValue* denied_sites = update_denied_sites.Get();
    // |value| is passed to the preferences list, or deleted.
    StringValue* value = new StringValue(origin.spec());

    // Remove from one list and add to the other.
    if (is_allowed) {
      // Remove from the denied list.
      if (denied_sites->Remove(*value) != -1)
        denied_changed = true;

      // Add to the allowed list.
      if (allowed_sites->AppendIfNotPresent(value))
        allowed_changed = true;
    } else {
      // Remove from the allowed list.
      if (allowed_sites->Remove(*value) != -1)
        allowed_changed = true;

      // Add to the denied list.
      if (denied_sites->AppendIfNotPresent(value))
        denied_changed = true;
    }
  }

  // Persist the pref if anthing changed, but only send updates for the
  // list that changed.
  if (allowed_changed || denied_changed)
    prefs->ScheduleSavePersistentPrefs();
  StartObserving();
}

ContentSetting NotificationProvider::GetContentSetting(
    const GURL& origin) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (profile_->IsOffTheRecord())
    return kDefaultSetting;

  std::vector<GURL> allowed_origins(GetAllowedOrigins());
  if (std::find(allowed_origins.begin(), allowed_origins.end(), origin) !=
      allowed_origins.end())
    return CONTENT_SETTING_ALLOW;

  std::vector<GURL> denied_origins(GetBlockedOrigins());
  if (std::find(denied_origins.begin(), denied_origins.end(), origin) !=
      denied_origins.end())
    return CONTENT_SETTING_BLOCK;

  return CONTENT_SETTING_DEFAULT;
}

void NotificationProvider::ResetAllowedOrigin(const GURL& origin) {
  if (profile_->IsOffTheRecord())
    return;

  // Since this isn't called often, let the normal observer behavior update the
  // cache in this case.
  PrefService* prefs = profile_->GetPrefs();
  {
    ListPrefUpdate update(prefs, prefs::kDesktopNotificationAllowedOrigins);
    ListValue* allowed_sites = update.Get();
    StringValue value(origin.spec());
    int removed_index = allowed_sites->Remove(value);
    DCHECK_NE(-1, removed_index) << origin << " was not allowed";
  }
  prefs->ScheduleSavePersistentPrefs();
}

void NotificationProvider::ResetBlockedOrigin(const GURL& origin) {
  if (profile_->IsOffTheRecord())
    return;

  // Since this isn't called often, let the normal observer behavior update the
  // cache in this case.
  PrefService* prefs = profile_->GetPrefs();
  {
    ListPrefUpdate update(prefs, prefs::kDesktopNotificationDeniedOrigins);
    ListValue* denied_sites = update.Get();
    StringValue value(origin.spec());
    int removed_index = denied_sites->Remove(value);
    DCHECK_NE(-1, removed_index) << origin << " was not blocked";
  }
  prefs->ScheduleSavePersistentPrefs();
}

void NotificationProvider::ResetAllOrigins() {
  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kDesktopNotificationAllowedOrigins);
  prefs->ClearPref(prefs::kDesktopNotificationDeniedOrigins);
}

}  // namespace content_settings
