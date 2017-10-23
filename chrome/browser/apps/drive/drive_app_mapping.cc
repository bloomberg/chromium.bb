// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_mapping.h"

#include <stddef.h>

#include <memory>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Key for a string value that holds the mapped chrome app id.
const char kKeyChromeApp[] = "chrome_app";

// Key for a boolean value of whether the mapped chrome app is auto generated.
// Default is false.
const char kKeyGenerated[] = "generated";

std::unique_ptr<base::DictionaryValue> CreateInfoDict(
    const std::string& chrome_app_id,
    bool generated) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetKey(kKeyChromeApp, base::Value(chrome_app_id));

  // Only writes non-default value.
  if (generated)
    dict->SetKey(kKeyGenerated, base::Value(true));
  return dict;
}

}  // namespace

DriveAppMapping::DriveAppMapping(PrefService* prefs) : prefs_(prefs) {
  GetUninstalledIdsFromPref();
}

DriveAppMapping::~DriveAppMapping() {
}

// static
void DriveAppMapping::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kAppLauncherDriveAppMapping);
  registry->RegisterListPref(prefs::kAppLauncherUninstalledDriveApps);
}

void DriveAppMapping::Add(const std::string& drive_app_id,
                          const std::string& chrome_app_id,
                          bool generated) {
  DictionaryPrefUpdate update(prefs_, prefs::kAppLauncherDriveAppMapping);
  update->SetWithoutPathExpansion(drive_app_id,
                                  CreateInfoDict(chrome_app_id, generated));
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
    DCHECK(it.value().is_dict());
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
    DCHECK(it.value().is_dict());
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

void DriveAppMapping::AddUninstalledDriveApp(const std::string& drive_app_id) {
  if (IsUninstalledDriveApp(drive_app_id))
    return;
  uninstalled_app_ids_.insert(drive_app_id);
  UpdateUninstalledList();
}

void DriveAppMapping::RemoveUninstalledDriveApp(
    const std::string& drive_app_id) {
  auto it = uninstalled_app_ids_.find(drive_app_id);
  if (it == uninstalled_app_ids_.end())
    return;
  uninstalled_app_ids_.erase(it);
  UpdateUninstalledList();
}

bool DriveAppMapping::IsUninstalledDriveApp(
    const std::string& drive_app_id) const {
  return uninstalled_app_ids_.find(drive_app_id) != uninstalled_app_ids_.end();
}

void DriveAppMapping::GetUninstalledIdsFromPref() {
  uninstalled_app_ids_.clear();
  const base::ListValue* list =
      prefs_->GetList(prefs::kAppLauncherUninstalledDriveApps);
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string app_id;
    if (!list->GetString(i, &app_id))
      continue;
    uninstalled_app_ids_.insert(app_id);
  }
}

void DriveAppMapping::UpdateUninstalledList() {
  ListPrefUpdate update(prefs_, prefs::kAppLauncherUninstalledDriveApps);
  update->Clear();
  for (const auto& app_id : uninstalled_app_ids_)
    update->AppendString(app_id);
}
