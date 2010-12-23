// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  pref_store_.reset(pref_store);
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

bool PrefValueStore::GetValue(const std::string& name,
                              Value** out_value) const {
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (GetValueFromStore(name.c_str(), static_cast<PrefStoreType>(i),
                          out_value))
      return true;
  }
  return false;
}

void PrefValueStore::RegisterPreferenceType(const std::string& name,
                                            Value::ValueType type) {
  pref_types_[name] = type;
}

Value::ValueType PrefValueStore::GetRegisteredType(
    const std::string& name) const {
  PrefTypeMap::const_iterator found = pref_types_.find(name);
  if (found == pref_types_.end())
    return Value::TYPE_NULL;
  return found->second;
}

bool PrefValueStore::HasPrefPath(const char* path) const {
  Value* tmp_value = NULL;
  const std::string name(path);
  bool rv = GetValue(name, &tmp_value);
  // Merely registering a pref doesn't count as "having" it: we require a
  // non-default value set.
  return rv && !PrefValueFromDefaultStore(path);
}

void PrefValueStore::NotifyPrefChanged(
    const char* path,
    PrefValueStore::PrefStoreType new_store) {
  DCHECK(new_store != INVALID_STORE);

  // If this pref is not registered, just discard the notification.
  if (!pref_types_.count(path))
    return;

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

// Returns true if the actual value is a valid type for the expected type when
// found in the given store.
bool PrefValueStore::IsValidType(Value::ValueType expected,
                                 Value::ValueType actual,
                                 PrefValueStore::PrefStoreType store) {
  if (expected == actual)
    return true;

  // Dictionaries and lists are allowed to hold TYPE_NULL values too, but only
  // in the default pref store.
  if (store == DEFAULT_STORE &&
      actual == Value::TYPE_NULL &&
      (expected == Value::TYPE_DICTIONARY || expected == Value::TYPE_LIST)) {
    return true;
  }
  return false;
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
        if (IsValidType(GetRegisteredType(name),
                        (*out_value)->GetType(),
                        store_type)) {
          return true;
        }
        break;
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
    PrefStore* store = GetPrefStore(static_cast<PrefStoreType>(i));
    if (store && !store->IsInitializationComplete())
      return;
  }
  pref_notifier_->OnInitializationCompleted();
}
