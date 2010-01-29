// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

HostContentSettingsMap::HostContentSettingsMap(Profile* profile)
    : profile_(profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  const DictionaryValue* host_content_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kPerHostContentSettings);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (host_content_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(host_content_dictionary->begin_keys());
         i != host_content_dictionary->end_keys(); ++i) {
      std::wstring wide_host(*i);
      int content_settings = 0;
      bool success = host_content_dictionary->GetIntegerWithoutPathExpansion(
          wide_host, &content_settings);
      DCHECK(success);
      host_content_settings_[WideToUTF8(wide_host)] =
        ContentPermissions::FromInteger(content_settings);
    }
  }
  default_content_settings_ = ContentPermissions::FromInteger(
      profile_->GetPrefs()->GetInteger(prefs::kDefaultContentSettings));
}

// static
void HostContentSettingsMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kPerHostContentSettings);
  prefs->RegisterIntegerPref(prefs::kDefaultContentSettings,
                             ContentPermissions::ToInteger(
                             ContentPermissions()));
}

void HostContentSettingsMap::ResetToDefaults() {
  default_content_settings_ = ContentPermissions();
  host_content_settings_.clear();
  profile_->GetPrefs()->ClearPref(prefs::kDefaultContentSettings);
  profile_->GetPrefs()->ClearPref(prefs::kPerHostContentSettings);
}

HostContentSettingsMap::HostContentPermissions
    HostContentSettingsMap::GetAllPerHostContentPermissions(
    ContentSettingsType content_type) const {
  HostContentPermissions result;
  for (HostContentSettings::const_iterator i(host_content_settings_.begin());
       i != host_content_settings_.end(); ++i)
    if (i->second.permissions[content_type] !=
        CONTENT_PERMISSION_TYPE_DEFAULT)
      result[i->first] = i->second.permissions[content_type];
  return result;
}

ContentPermissions HostContentSettingsMap::GetPerHostContentSettings(
    const std::string& host) const {
  AutoLock auto_lock(lock_);
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  ContentPermissions result = default_content_settings_;
  if (i != host_content_settings_.end()) {
    for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
      if (i->second.permissions[j] != CONTENT_PERMISSION_TYPE_DEFAULT)
        result.permissions[j] = i->second.permissions[j];
  }
  return result;
}

bool HostContentSettingsMap::SetDefaultContentPermission(
    ContentSettingsType type, ContentPermissionType permission) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (permission == CONTENT_PERMISSION_TYPE_DEFAULT)
    return false;

  {
    AutoLock auto_lock(lock_);
    default_content_settings_.permissions[type] = permission;
  }

  // Persist new content settings if we're not off the record.
  if (!profile_->IsOffTheRecord()) {
    profile_->GetPrefs()->SetInteger(prefs::kDefaultContentSettings,
        ContentPermissions::ToInteger(default_content_settings_));
  }
  return true;
}

void HostContentSettingsMap::SetPerHostContentPermission(
    const std::string& host, ContentSettingsType type,
    ContentPermissionType permission) {

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (host.empty())
    return;

  bool erase_entry = true;
  ContentPermissions permissions;

  {
    AutoLock auto_lock(lock_);
    HostContentSettings::const_iterator i(host_content_settings_.find(host));
    if (i == host_content_settings_.end()) {
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
        permissions.permissions[j] = CONTENT_PERMISSION_TYPE_DEFAULT;
    } else {
      permissions = i->second;
    }
    permissions.permissions[type] = permission;
    for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
      if (permissions.permissions[j] != CONTENT_PERMISSION_TYPE_DEFAULT)
        erase_entry = false;
    if (erase_entry)
      host_content_settings_.erase(host);
    else
      host_content_settings_[host] = permissions;
  }

  // Persist new content settings if we're not off the record.
  if (!profile_->IsOffTheRecord()) {
    DictionaryValue* host_content_dictionary =
        profile_->GetPrefs()->GetMutableDictionary(
            prefs::kPerHostContentSettings);
    std::wstring wide_host(UTF8ToWide(host));
    if (erase_entry) {
      host_content_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
    } else {
      host_content_dictionary->SetWithoutPathExpansion(wide_host,
        Value::CreateIntegerValue(ContentPermissions::ToInteger(permissions)));
    }
  }
}

void HostContentSettingsMap::SetPerHostContentSettings(const std::string& host,
    const ContentPermissions& permissions) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (host.empty())
    return;

  bool erase_entry = true;

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    if (permissions.permissions[i] != CONTENT_PERMISSION_TYPE_DEFAULT)
      erase_entry = false;

  {
    AutoLock auto_lock(lock_);
    if (erase_entry)
      host_content_settings_.erase(host);
    else
      host_content_settings_[host] = permissions;
  }

  // Persist new content settings if we're not off the record.
  if (!profile_->IsOffTheRecord()) {
    DictionaryValue* host_content_dictionary =
        profile_->GetPrefs()->GetMutableDictionary(
            prefs::kPerHostContentSettings);
    std::wstring wide_host(UTF8ToWide(host));
    if (erase_entry) {
      host_content_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
    } else {
      host_content_dictionary->SetWithoutPathExpansion(wide_host,
        Value::CreateIntegerValue(ContentPermissions::ToInteger(permissions)));
    }
  }
}
