// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_value_store.h"

#include "chrome/browser/prefs/pref_notifier.h"

PrefValueStore::PrefStoreKeeper::PrefStoreKeeper()
    : pref_value_store_(NULL),
      type_(PrefValueStore::INVALID_STORE) {
}

PrefValueStore::PrefStoreKeeper::~PrefStoreKeeper() {
  if (pref_store_.get())
    pref_store_->RemoveObserver(this);
}

void PrefValueStore::PrefStoreKeeper::Initialize(
    PrefValueStore* store,
    PrefStore* pref_store,
    PrefValueStore::PrefStoreType type) {
  if (pref_store_.get())
    pref_store_->RemoveObserver(this);
  type_ = type;
  pref_value_store_ = store;
  pref_store_ = pref_store;
  if (pref_store_.get())
    pref_store_->AddObserver(this);
}

void PrefValueStore::PrefStoreKeeper::OnPrefValueChanged(
    const std::string& key) {
  pref_value_store_->OnPrefValueChanged(type_, key);
}

void PrefValueStore::PrefStoreKeeper::OnInitializationCompleted() {
  pref_value_store_->OnInitializationCompleted(type_);
}

PrefValueStore::PrefValueStore(PrefStore* managed_platform_prefs,
                               PrefStore* device_management_prefs,
                               PrefStore* extension_prefs,
                               PrefStore* command_line_prefs,
                               PrefStore* user_prefs,
                               PrefStore* recommended_prefs,
                               PrefStore* default_prefs,
                               PrefNotifier* pref_notifier)
    : pref_notifier_(pref_notifier) {
  InitPrefStore(MANAGED_PLATFORM_STORE, managed_platform_prefs);
  InitPrefStore(DEVICE_MANAGEMENT_STORE, device_management_prefs);
  InitPrefStore(EXTENSION_STORE, extension_prefs);
  InitPrefStore(COMMAND_LINE_STORE, command_line_prefs);
  InitPrefStore(USER_STORE, user_prefs);
  InitPrefStore(RECOMMENDED_STORE, recommended_prefs);
  InitPrefStore(DEFAULT_STORE, default_prefs);

  CheckInitializationCompleted();
}

PrefValueStore::~PrefValueStore() {}

PrefValueStore* PrefValueStore::CloneAndSpecialize(
    PrefStore* managed_platform_prefs,
    PrefStore* device_management_prefs,
    PrefStore* extension_prefs,
    PrefStore* command_line_prefs,
    PrefStore* user_prefs,
    PrefStore* recommended_prefs,
    PrefStore* default_prefs,
    PrefNotifier* pref_notifier) {
  DCHECK(pref_notifier);
  if (!managed_platform_prefs)
    managed_platform_prefs = GetPrefStore(MANAGED_PLATFORM_STORE);
  if (!device_management_prefs)
    device_management_prefs = GetPrefStore(DEVICE_MANAGEMENT_STORE);
  if (!extension_prefs)
    extension_prefs = GetPrefStore(EXTENSION_STORE);
  if (!command_line_prefs)
    command_line_prefs = GetPrefStore(COMMAND_LINE_STORE);
  if (!user_prefs)
    user_prefs = GetPrefStore(USER_STORE);
  if (!recommended_prefs)
    recommended_prefs = GetPrefStore(RECOMMENDED_STORE);
  if (!default_prefs)
    default_prefs = GetPrefStore(DEFAULT_STORE);

  return new PrefValueStore(
      managed_platform_prefs, device_management_prefs, extension_prefs,
      command_line_prefs, user_prefs, recommended_prefs, default_prefs,
      pref_notifier);
}

bool PrefValueStore::GetValue(const std::string& name,
                              Value::ValueType type,
                              Value** out_value) const {
  *out_value = NULL;
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (GetValueFromStore(name.c_str(), static_cast<PrefStoreType>(i),
                          out_value)) {
      if (!(*out_value)->IsType(type)) {
        LOG(WARNING) << "Expected type for " << name << " is " << type
                     << " but got " << (*out_value)->GetType()
                     << " in store " << i;
        continue;
      }
      return true;
    }
  }
  return false;
}

void PrefValueStore::NotifyPrefChanged(
    const char* path,
    PrefValueStore::PrefStoreType new_store) {
  DCHECK(new_store != INVALID_STORE);

  bool changed = true;
  // Replying that the pref has changed in case the new store is invalid may
  // cause problems, but it's the safer choice.
  if (new_store != INVALID_STORE) {
    PrefStoreType controller = ControllingPrefStoreForPref(path);
    DCHECK(controller != INVALID_STORE);
    // If the pref is controlled by a higher-priority store, its effective value
    // cannot have changed.
    if (controller != INVALID_STORE &&
        controller < new_store) {
      changed = false;
    }
  }

  if (changed)
    pref_notifier_->OnPreferenceChanged(path);
}

bool PrefValueStore::PrefValueInManagedPlatformStore(const char* name) const {
  return PrefValueInStore(name, MANAGED_PLATFORM_STORE);
}

bool PrefValueStore::PrefValueInDeviceManagementStore(const char* name) const {
  return PrefValueInStore(name, DEVICE_MANAGEMENT_STORE);
}

bool PrefValueStore::PrefValueInExtensionStore(const char* name) const {
  return PrefValueInStore(name, EXTENSION_STORE);
}

bool PrefValueStore::PrefValueInUserStore(const char* name) const {
  return PrefValueInStore(name, USER_STORE);
}

bool PrefValueStore::PrefValueFromExtensionStore(const char* name) const {
  return ControllingPrefStoreForPref(name) == EXTENSION_STORE;
}

bool PrefValueStore::PrefValueFromUserStore(const char* name) const {
  return ControllingPrefStoreForPref(name) == USER_STORE;
}

bool PrefValueStore::PrefValueFromDefaultStore(const char* name) const {
  return ControllingPrefStoreForPref(name) == DEFAULT_STORE;
}

bool PrefValueStore::PrefValueUserModifiable(const char* name) const {
  PrefStoreType effective_store = ControllingPrefStoreForPref(name);
  return effective_store >= USER_STORE ||
         effective_store == INVALID_STORE;
}

bool PrefValueStore::PrefValueInStore(
    const char* name,
    PrefValueStore::PrefStoreType store) const {
  // Declare a temp Value* and call GetValueFromStore,
  // ignoring the output value.
  Value* tmp_value = NULL;
  return GetValueFromStore(name, store, &tmp_value);
}

bool PrefValueStore::PrefValueInStoreRange(
    const char* name,
    PrefValueStore::PrefStoreType first_checked_store,
    PrefValueStore::PrefStoreType last_checked_store) const {
  if (first_checked_store > last_checked_store) {
    NOTREACHED();
    return false;
  }

  for (size_t i = first_checked_store;
       i <= static_cast<size_t>(last_checked_store); ++i) {
    if (PrefValueInStore(name, static_cast<PrefStoreType>(i)))
      return true;
  }
  return false;
}

PrefValueStore::PrefStoreType PrefValueStore::ControllingPrefStoreForPref(
    const char* name) const {
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (PrefValueInStore(name, static_cast<PrefStoreType>(i)))
      return static_cast<PrefStoreType>(i);
  }
  return INVALID_STORE;
}

bool PrefValueStore::GetValueFromStore(const char* name,
                                       PrefValueStore::PrefStoreType store_type,
                                       Value** out_value) const {
  // Only return true if we find a value and it is the correct type, so stale
  // values with the incorrect type will be ignored.
  const PrefStore* store = GetPrefStore(static_cast<PrefStoreType>(store_type));
  if (store) {
    switch (store->GetValue(name, out_value)) {
      case PrefStore::READ_USE_DEFAULT:
        store = GetPrefStore(DEFAULT_STORE);
        if (!store || store->GetValue(name, out_value) != PrefStore::READ_OK) {
          *out_value = NULL;
          return false;
        }
        // Fall through...
      case PrefStore::READ_OK:
        return true;
      case PrefStore::READ_NO_VALUE:
        break;
    }
  }

  // No valid value found for the given preference name: set the return false.
  *out_value = NULL;
  return false;
}

void PrefValueStore::OnPrefValueChanged(PrefValueStore::PrefStoreType type,
                                        const std::string& key) {
  NotifyPrefChanged(key.c_str(), type);
}

void PrefValueStore::OnInitializationCompleted(
    PrefValueStore::PrefStoreType type) {
  CheckInitializationCompleted();
}

void PrefValueStore::InitPrefStore(PrefValueStore::PrefStoreType type,
                                   PrefStore* pref_store) {
  pref_stores_[type].Initialize(this, pref_store, type);
}

void PrefValueStore::CheckInitializationCompleted() {
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    scoped_refptr<PrefStore> store =
        GetPrefStore(static_cast<PrefStoreType>(i));
    if (store && !store->IsInitializationComplete())
      return;
  }
  pref_notifier_->OnInitializationCompleted();
}
