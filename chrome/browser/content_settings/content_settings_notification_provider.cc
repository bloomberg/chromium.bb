// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/content_settings/content_settings_notification_provider.h"

#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"

namespace {

const std::string HTTP_PREFIX("http://");
const ContentSetting kDefaultSetting = CONTENT_SETTING_ASK;

}  // namespace

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// DefaultNotificationProvider
//

// static
void NotificationDefaultProvider::RegisterUserPrefs(PrefService* user_prefs) {
  if (!user_prefs->FindPreference(
      prefs::kDesktopNotificationDefaultContentSetting)) {
    user_prefs->RegisterIntegerPref(
        prefs::kDesktopNotificationDefaultContentSetting, kDefaultSetting);
  }
}

NotificationDefaultProvider::NotificationDefaultProvider(
    Profile* profile)
    : profile_(profile) {
  prefs_registrar_.Init(profile_->GetPrefs());
  StartObserving();
}

NotificationDefaultProvider::~NotificationDefaultProvider() {
  StopObserving();
}

ContentSetting NotificationDefaultProvider::ProvideDefaultSetting(
    ContentSettingsType content_type) const {
  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    PrefService* prefs = profile_->GetPrefs();
    ContentSetting setting = IntToContentSetting(
        prefs->GetInteger(prefs::kDesktopNotificationDefaultContentSetting));
    if (setting == CONTENT_SETTING_DEFAULT)
      setting = kDefaultSetting;
    return setting;
  }
  return CONTENT_SETTING_DEFAULT;
}

void NotificationDefaultProvider::UpdateDefaultSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    profile_->GetPrefs()->SetInteger(
        prefs::kDesktopNotificationDefaultContentSetting,
        setting == CONTENT_SETTING_DEFAULT ?  kDefaultSetting : setting);
  }
}

void NotificationDefaultProvider::ResetToDefaults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kDesktopNotificationDefaultContentSetting);
}

bool NotificationDefaultProvider::DefaultSettingIsManaged(
    ContentSettingsType content_type) const {
  if (content_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    return profile_->GetPrefs()->IsManagedPreference(
        prefs::kDesktopNotificationDefaultContentSetting);
  }
  return false;
}

void NotificationDefaultProvider::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    const std::string& name = *Details<std::string>(details).ptr();
    OnPrefsChanged(name);
  } else if (NotificationType::PROFILE_DESTROYED == type) {
    StopObserving();
  }
}

void NotificationDefaultProvider::StartObserving() {
  if (!profile_->IsOffTheRecord()) {
    prefs_registrar_.Add(prefs::kDesktopNotificationDefaultContentSetting,
                         this);
  }
  notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                              Source<Profile>(profile_));
}

void NotificationDefaultProvider::StopObserving() {
  if (!profile_->IsOffTheRecord()) {
    prefs_registrar_.RemoveAll();
  }
  notification_registrar_.RemoveAll();
}

void NotificationDefaultProvider::OnPrefsChanged(const std::string& pref_name) {
  if (pref_name == prefs::kDesktopNotificationDefaultContentSetting) {
    NotificationService::current()->Notify(
        NotificationType::DESKTOP_NOTIFICATION_DEFAULT_CHANGED,
        Source<DesktopNotificationService>(
            profile_->GetDesktopNotificationService()),
        NotificationService::NoDetails());
  }
}

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

  std::string origin_str(requesting_url_pattern.AsString());

  if (0 == origin_str.find_first_of(ContentSettingsPattern::kDomainWildcard)) {
    NOTREACHED();
  }

  GURL origin(HTTP_PREFIX + origin_str);
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
          profile_->GetDesktopNotificationService()),
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
  // Don't persist changes when off the record.
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

  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  {
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
  if (allowed_changed || denied_changed) {
    if (allowed_changed) {
      ScopedUserPrefUpdate update_allowed(
          prefs, prefs::kDesktopNotificationAllowedOrigins);
    }
    if (denied_changed) {
      ScopedUserPrefUpdate updateDenied(
          prefs, prefs::kDesktopNotificationDeniedOrigins);
    }
    prefs->ScheduleSavePersistentPrefs();
  }
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
  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  {
    StringValue value(origin.spec());
    int removed_index = allowed_sites->Remove(value);
    DCHECK_NE(-1, removed_index) << origin << " was not allowed";
    ScopedUserPrefUpdate update_allowed(
        prefs, prefs::kDesktopNotificationAllowedOrigins);
  }
  prefs->ScheduleSavePersistentPrefs();
}

void NotificationProvider::ResetBlockedOrigin(const GURL& origin) {
  if (profile_->IsOffTheRecord())
    return;

  // Since this isn't called often, let the normal observer behavior update the
  // cache in this case.
  PrefService* prefs = profile_->GetPrefs();
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  {
    StringValue value(origin.spec());
    int removed_index = denied_sites->Remove(value);
    DCHECK_NE(-1, removed_index) << origin << " was not blocked";
    ScopedUserPrefUpdate update_allowed(
        prefs, prefs::kDesktopNotificationDeniedOrigins);
  }
  prefs->ScheduleSavePersistentPrefs();
}

void NotificationProvider::ResetAllOrigins() {
  PrefService* prefs = profile_->GetPrefs();
  prefs->ClearPref(prefs::kDesktopNotificationAllowedOrigins);
  prefs->ClearPref(prefs::kDesktopNotificationDeniedOrigins);
}

}  // namespace content_settings
