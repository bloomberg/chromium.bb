// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_value_map.h"

#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_value_map.h"

struct ExtensionPrefValueMap::ExtensionEntry {
  // Installation time of the extension.
  base::Time install_time;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Regular preferences.
  PrefValueMap reg_preferences;
  // Incognito preferences, empty for regular ExtensionPrefStore.
  PrefValueMap inc_preferences;
};

ExtensionPrefValueMap::ExtensionPrefValueMap() {
}

ExtensionPrefValueMap::~ExtensionPrefValueMap() {
  NotifyOfDestruction();
  STLDeleteValues(&entries_);
  entries_.clear();
}

void ExtensionPrefValueMap::SetExtensionPref(const std::string& ext_id,
                                             const std::string& key,
                                             bool incognito,
                                             Value* value) {
  PrefValueMap* prefs = GetExtensionPrefValueMap(ext_id, incognito);

  if (prefs->SetValue(key, value))
    NotifyPrefValueChanged(key);
}

void ExtensionPrefValueMap::RemoveExtensionPref(const std::string& ext_id,
                                                const std::string& key,
                                                bool incognito) {
  PrefValueMap* prefs = GetExtensionPrefValueMap(ext_id, incognito);
  if (prefs->RemoveValue(key))
    NotifyPrefValueChanged(key);
}

void ExtensionPrefValueMap::RegisterExtension(const std::string& ext_id,
                                              const base::Time& install_time,
                                              bool is_enabled) {
  if (entries_.find(ext_id) != entries_.end())
    UnregisterExtension(ext_id);
  entries_[ext_id] = new ExtensionEntry;
  entries_[ext_id]->install_time = install_time;
  entries_[ext_id]->enabled = is_enabled;
}

void ExtensionPrefValueMap::UnregisterExtension(const std::string& ext_id) {
  ExtensionEntryMap::iterator i = entries_.find(ext_id);
  if (i == entries_.end())
    return;
  std::set<std::string> keys;  // keys set by this extension
  GetExtensionControlledKeys(*(i->second), &keys);

  delete i->second;
  entries_.erase(i);

  NotifyPrefValueChanged(keys);
}

void ExtensionPrefValueMap::SetExtensionState(const std::string& ext_id,
                                              bool is_enabled) {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  CHECK(i != entries_.end());
  if (i->second->enabled == is_enabled)
    return;
  std::set<std::string> keys;  // keys set by this extension
  GetExtensionControlledKeys(*(i->second), &keys);
  i->second->enabled = is_enabled;
  NotifyPrefValueChanged(keys);
}

PrefValueMap* ExtensionPrefValueMap::GetExtensionPrefValueMap(
    const std::string& ext_id,
    bool incognito) {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  CHECK(i != entries_.end());
  return incognito ? &(i->second->inc_preferences)
                   : &(i->second->reg_preferences);
}

const PrefValueMap* ExtensionPrefValueMap::GetExtensionPrefValueMap(
    const std::string& ext_id,
    bool incognito) const {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  CHECK(i != entries_.end());
  return incognito ? &(i->second->inc_preferences)
                   : &(i->second->reg_preferences);
}

void ExtensionPrefValueMap::GetExtensionControlledKeys(
    const ExtensionEntry& entry,
    std::set<std::string>* out) const {
  PrefValueMap::const_iterator i;

  const PrefValueMap& reg_prefs = entry.reg_preferences;
  for (i = reg_prefs.begin(); i != reg_prefs.end(); ++i)
    out->insert(i->first);

  const PrefValueMap& inc_prefs = entry.inc_preferences;
  for (i = inc_prefs.begin(); i != inc_prefs.end(); ++i)
    out->insert(i->first);
}

const Value* ExtensionPrefValueMap::GetEffectivePrefValue(
    const std::string& key,
    bool incognito) const {
  Value *winner = NULL;
  base::Time winners_install_time;

  ExtensionEntryMap::const_iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i) {
    const std::string& ext_id = i->first;
    const base::Time& install_time = i->second->install_time;
    const bool enabled = i->second->enabled;

    if (!enabled)
      continue;
    if (install_time < winners_install_time)
      continue;

    Value* value = NULL;
    const PrefValueMap* prefs = GetExtensionPrefValueMap(ext_id, false);
    if (prefs->GetValue(key, &value)) {
      winner = value;
      winners_install_time = install_time;
    }

    if (!incognito)
      continue;

    prefs = GetExtensionPrefValueMap(ext_id, true);
    if (prefs->GetValue(key, &value)) {
      winner = value;
      winners_install_time = install_time;
    }
  }
  return winner;
}

void ExtensionPrefValueMap::AddObserver(
    ExtensionPrefValueMap::Observer* observer) {
  observers_.AddObserver(observer);

  // Collect all currently used keys and notify the new observer.
  std::set<std::string> keys;
  ExtensionEntryMap::const_iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i)
    GetExtensionControlledKeys(*(i->second), &keys);

  std::set<std::string>::const_iterator j;
  for (j = keys.begin(); j != keys.end(); ++j)
    observer->OnPrefValueChanged(*j);
}

void ExtensionPrefValueMap::RemoveObserver(
    ExtensionPrefValueMap::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionPrefValueMap::NotifyInitializationCompleted() {
  FOR_EACH_OBSERVER(ExtensionPrefValueMap::Observer, observers_,
                    OnInitializationCompleted());
}

void ExtensionPrefValueMap::NotifyPrefValueChanged(
    const std::set<std::string>& keys) {
  std::set<std::string>::const_iterator i;
  for (i = keys.begin(); i != keys.end(); ++i)
    NotifyPrefValueChanged(*i);
}

void ExtensionPrefValueMap::NotifyPrefValueChanged(const std::string& key) {
  FOR_EACH_OBSERVER(ExtensionPrefValueMap::Observer, observers_,
                    OnPrefValueChanged(key));
}

void ExtensionPrefValueMap::NotifyOfDestruction() {
  FOR_EACH_OBSERVER(ExtensionPrefValueMap::Observer, observers_,
                    OnExtensionPrefValueMapDestruction());
}
