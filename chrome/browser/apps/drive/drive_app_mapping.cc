// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_mapping.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

// Key for a string value that holds the mapped chrome app id.
const char kKeyChromeApp[] = "chrome_app";

// Key for a boolean value of whether the mapped chrome app is auto generated.
// Default is false.
const char kKeyGenerated[] = "generated";

scoped_ptr<base::DictionaryValue> CreateInfoDict(
    const std::string& chrome_app_id,
    bool generated) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetStringWithoutPathExpansion(kKeyChromeApp, chrome_app_id);

  // Only writes non-default value.
  if (generated)
    dict->SetBooleanWithoutPathExpansion(kKeyGenerated, true);
  return dict.Pass();
}

}  // namespace

DriveAppMapping::DriveAppMapping(PrefService* prefs) : prefs_(prefs) {
}

DriveAppMapping::~DriveAppMapping() {
}

// static
void DriveAppMapping::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kAppLauncherDriveAppMapping,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void DriveAppMapping::Add(const std::string& drive_app_id,
                          const std::string& chrome_app_id,
                          bool generated) {
  DictionaryPrefUpdate update(prefs_, prefs::kAppLauncherDriveAppMapping);
  update->SetWithoutPathExpansion(
      drive_app_id, CreateInfoDict(chrome_app_id, generated).release());
}

void DriveAppMapping::Remove(const std::string& drive_app_id) {
  DictionaryPrefUpdate update(prefs_, prefs::kAppLauncherDriveAppMapping);
  update->RemoveWithoutPathExpansion(drive_app_id, NULL);
}

std::string DriveAppMapping::GetChromeApp(
    const std::string& drive_app_id) const {
  const base::DictionaryValue* mapping =
      prefs_->GetDictionary(prefs::kAppLauncherDriveAppMapping);
  const base::DictionaryValue* info_dict;
  std::string chrome_app_id;
  if (mapping->GetDictionaryWithoutPathExpansion(drive_app_id, &info_dict) &&
      info_dict->GetStringWithoutPathExpansion(kKeyChromeApp, &chrome_app_id)) {
    return chrome_app_id;
  }

  return std::string();
}

std::string DriveAppMapping::GetDriveApp(
    const std::string& chrome_app_id) const {
  const base::DictionaryValue* mapping =
      prefs_->GetDictionary(prefs::kAppLauncherDriveAppMapping);
  for (base::DictionaryValue::Iterator it(*mapping); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* info_dict;
    std::string value_string;
    DCHECK(it.value().IsType(base::Value::TYPE_DICTIONARY));
    if (it.value().GetAsDictionary(&info_dict) &&
        info_dict->GetStringWithoutPathExpansion(kKeyChromeApp,
                                                 &value_string) &&
        value_string == chrome_app_id) {
      return it.key();
    }
  }
  return std::string();
}

bool DriveAppMapping::IsChromeAppGenerated(
    const std::string& chrome_app_id) const {
  const base::DictionaryValue* mapping =
      prefs_->GetDictionary(prefs::kAppLauncherDriveAppMapping);
  for (base::DictionaryValue::Iterator it(*mapping); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* info_dict;
    std::string value_string;
    bool generated = false;
    DCHECK(it.value().IsType(base::Value::TYPE_DICTIONARY));
    if (it.value().GetAsDictionary(&info_dict) &&
        info_dict->GetStringWithoutPathExpansion(kKeyChromeApp,
                                                 &value_string) &&
        value_string == chrome_app_id &&
        info_dict->GetBooleanWithoutPathExpansion(kKeyGenerated, &generated)) {
      return generated;
    }
  }

  return false;
}

std::set<std::string> DriveAppMapping::GetDriveAppIds() const {
  const base::DictionaryValue* mapping =
      prefs_->GetDictionary(prefs::kAppLauncherDriveAppMapping);
  std::set<std::string> keys;
  for (base::DictionaryValue::Iterator it(*mapping); !it.IsAtEnd();
       it.Advance()) {
    keys.insert(it.key());
  }
  return keys;
}
