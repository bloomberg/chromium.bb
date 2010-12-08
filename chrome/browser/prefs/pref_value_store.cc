// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_value_store.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/common/notification_service.h"

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
                               PrefNotifier* pref_notifier,
                               Profile* profile)
    : pref_notifier_(pref_notifier),
      profile_(profile) {
  // NULL default pref store is usually bad, but may be OK for some unit tests.
  if (!default_prefs)
    LOG(WARNING) << "default pref store is null";
  InitPrefStore(MANAGED_PLATFORM_STORE, managed_platform_prefs);
  InitPrefStore(DEVICE_MANAGEMENT_STORE, device_management_prefs);
  InitPrefStore(EXTENSION_STORE, extension_prefs);
  InitPrefStore(COMMAND_LINE_STORE, command_line_prefs);
  InitPrefStore(USER_STORE, user_prefs);
  InitPrefStore(RECOMMENDED_STORE, recommended_prefs);
  InitPrefStore(DEFAULT_STORE, default_prefs);

  // TODO(mnissler): Remove after policy refresh cleanup.
  registrar_.Add(this,
                 NotificationType(NotificationType::POLICY_CHANGED),
                 NotificationService::AllSources());

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

bool PrefValueStore::GetUserValue(const std::string& name,
                                  Value** out_value) const {
  return GetValueFromStore(name.c_str(), USER_STORE, out_value);
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

bool PrefValueStore::WritePrefs() {
  bool success = true;
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    PrefStore* store = GetPrefStore(static_cast<PrefStoreType>(i));
    if (store)
      success = store->WritePrefs() && success;
  }
  return success;
}

void PrefValueStore::ScheduleWritePrefs() {
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    PrefStore* store = GetPrefStore(static_cast<PrefStoreType>(i));
    if (store)
      store->ScheduleWritePrefs();
  }
}

PrefStore::PrefReadError PrefValueStore::ReadPrefs() {
  PrefStore::PrefReadError result = PrefStore::PREF_READ_ERROR_NONE;
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    PrefStore* store = GetPrefStore(static_cast<PrefStoreType>(i));
    if (store) {
      PrefStore::PrefReadError this_error = store->ReadPrefs();
      if (result == PrefStore::PREF_READ_ERROR_NONE)
        result = this_error;
    }
  }

  if (HasPolicyConflictingUserProxySettings()) {
    LOG(WARNING) << "user-requested proxy options have been overridden"
                 << " by a proxy configuration specified in a centrally"
                 << " administered policy.";
  }

  // TODO(markusheintz): Return a better error status: maybe a struct with
  // the error status of all PrefStores.
  return result;
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

void PrefValueStore::SetUserPrefValue(const char* name, Value* in_value) {
  DCHECK(in_value);
  Value* old_value = NULL;
  GetPrefStore(USER_STORE)->prefs()->Get(name, &old_value);
  bool value_changed = !old_value || !old_value->Equals(in_value);
  GetPrefStore(USER_STORE)->prefs()->Set(name, in_value);

  if (value_changed)
    NotifyPrefChanged(name, USER_STORE);
}

void PrefValueStore::SetUserPrefValueSilently(const char* name,
                                              Value* in_value) {
  DCHECK(in_value);
  GetPrefStore(USER_STORE)->prefs()->Set(name, in_value);
}

bool PrefValueStore::ReadOnly() const {
  return GetPrefStore(USER_STORE)->ReadOnly();
}

void PrefValueStore::RemoveUserPrefValue(const char* name) {
  if (GetPrefStore(USER_STORE)) {
    if (GetPrefStore(USER_STORE)->prefs()->Remove(name, NULL))
      NotifyPrefChanged(name, USER_STORE);
  }
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

bool PrefValueStore::HasPolicyConflictingUserProxySettings() const {
  using policy::ConfigurationPolicyPrefStore;
  ConfigurationPolicyPrefStore::ProxyPreferenceSet proxy_prefs;
  ConfigurationPolicyPrefStore::GetProxyPreferenceSet(&proxy_prefs);
  ConfigurationPolicyPrefStore::ProxyPreferenceSet::const_iterator i;
  for (i = proxy_prefs.begin(); i != proxy_prefs.end(); ++i) {
    if ((PrefValueInManagedPlatformStore(*i) ||
         PrefValueInDeviceManagementStore(*i)) &&
        PrefValueInStoreRange(*i,
                              COMMAND_LINE_STORE,
                              USER_STORE))
      return true;
  }
  return false;
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
  if (store && store->prefs()->Get(name, out_value)) {
    // If the value is the sentinel that redirects to the default store,
    // re-fetch the value from the default store explicitly. Because the default
    // values are not available when creating stores, the default value must be
    // fetched dynamically for every redirect.
    if (PrefStore::IsUseDefaultSentinelValue(*out_value)) {
      store = GetPrefStore(DEFAULT_STORE);
      if (!store || !store->prefs()->Get(name, out_value)) {
        *out_value = NULL;
        return false;
      }
      store_type = DEFAULT_STORE;
    }
    if (IsValidType(GetRegisteredType(name),
                    (*out_value)->GetType(),
                    store_type)) {
      return true;
    }
  }
  // No valid value found for the given preference name: set the return false.
  *out_value = NULL;
  return false;
}

void PrefValueStore::RefreshPolicyPrefsOnFileThread(
    BrowserThread::ID calling_thread_id,
    PrefStore* new_managed_platform_pref_store,
    PrefStore* new_device_management_pref_store,
    PrefStore* new_recommended_pref_store) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<PrefStore> managed_platform_pref_store(
      new_managed_platform_pref_store);
  scoped_ptr<PrefStore> device_management_pref_store(
      new_device_management_pref_store);
  scoped_ptr<PrefStore> recommended_pref_store(new_recommended_pref_store);

  PrefStore::PrefReadError read_error =
      new_managed_platform_pref_store->ReadPrefs();
  if (read_error != PrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "refresh of managed policy failed: PrefReadError = "
               << read_error;
    return;
  }

  read_error = new_device_management_pref_store->ReadPrefs();
  if (read_error != PrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "refresh of device management policy failed: "
               << "PrefReadError = " << read_error;
    return;
  }

  read_error = new_recommended_pref_store->ReadPrefs();
  if (read_error != PrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "refresh of recommended policy failed: PrefReadError = "
               << read_error;
    return;
  }

  BrowserThread::PostTask(
      calling_thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &PrefValueStore::RefreshPolicyPrefsCompletion,
                        managed_platform_pref_store.release(),
                        device_management_pref_store.release(),
                        recommended_pref_store.release()));
}

void PrefValueStore::RefreshPolicyPrefs() {
  using policy::ConfigurationPolicyPrefStore;
  // Because loading of policy information must happen on the FILE
  // thread, it's not possible to just replace the contents of the
  // managed and recommended stores in place due to possible
  // concurrent access from the UI thread. Instead, new stores are
  // created and the refreshed policy read into them. The new stores
  // are swapped with the old from a Task on the UI thread after the
  // load is complete.
  PrefStore* new_managed_platform_pref_store(
      ConfigurationPolicyPrefStore::CreateManagedPlatformPolicyPrefStore());
  PrefStore* new_device_management_pref_store(
      ConfigurationPolicyPrefStore::CreateDeviceManagementPolicyPrefStore(
          profile_));
  PrefStore* new_recommended_pref_store(
      ConfigurationPolicyPrefStore::CreateRecommendedPolicyPrefStore());
  BrowserThread::ID current_thread_id;
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&current_thread_id));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &PrefValueStore::RefreshPolicyPrefsOnFileThread,
                        current_thread_id,
                        new_managed_platform_pref_store,
                        new_device_management_pref_store,
                        new_recommended_pref_store));
}

void PrefValueStore::RefreshPolicyPrefsCompletion(
    PrefStore* new_managed_platform_pref_store,
    PrefStore* new_device_management_pref_store,
    PrefStore* new_recommended_pref_store) {
  // Determine the paths of all the changed preferences values in the three
  // policy-related stores (managed platform, device management and
  // recommended).
  DictionaryValue* managed_platform_prefs_before(
      GetPrefStore(MANAGED_PLATFORM_STORE)->prefs());
  DictionaryValue* managed_platform_prefs_after(
      new_managed_platform_pref_store->prefs());
  DictionaryValue* device_management_prefs_before(
      GetPrefStore(DEVICE_MANAGEMENT_STORE)->prefs());
  DictionaryValue* device_management_prefs_after(
      new_device_management_pref_store->prefs());
  DictionaryValue* recommended_prefs_before(
      GetPrefStore(RECOMMENDED_STORE)->prefs());
  DictionaryValue* recommended_prefs_after(new_recommended_pref_store->prefs());

  std::vector<std::string> changed_managed_platform_paths;
  managed_platform_prefs_before->GetDifferingPaths(managed_platform_prefs_after,
                                          &changed_managed_platform_paths);

  std::vector<std::string> changed_device_management_paths;
  device_management_prefs_before->GetDifferingPaths(
      device_management_prefs_after,
      &changed_device_management_paths);

  std::vector<std::string> changed_recommended_paths;
  recommended_prefs_before->GetDifferingPaths(recommended_prefs_after,
                                              &changed_recommended_paths);

  // Merge all three vectors of changed value paths together, filtering
  // duplicates in a post-processing step.
  std::vector<std::string> all_changed_managed_platform_paths(
      changed_managed_platform_paths.size() +
      changed_device_management_paths.size());

  std::vector<std::string>::iterator last_insert =
      std::merge(changed_managed_platform_paths.begin(),
                 changed_managed_platform_paths.end(),
                 changed_device_management_paths.begin(),
                 changed_device_management_paths.end(),
                 all_changed_managed_platform_paths.begin());
  all_changed_managed_platform_paths.resize(
      last_insert - all_changed_managed_platform_paths.begin());

  std::vector<std::string> changed_paths(
      all_changed_managed_platform_paths.size() +
      changed_recommended_paths.size());
  last_insert = std::merge(all_changed_managed_platform_paths.begin(),
                           all_changed_managed_platform_paths.end(),
                           changed_recommended_paths.begin(),
                           changed_recommended_paths.end(),
                           changed_paths.begin());
  changed_paths.resize(last_insert - changed_paths.begin());

  last_insert = unique(changed_paths.begin(), changed_paths.end());
  changed_paths.resize(last_insert - changed_paths.begin());

  // Replace the old stores with the new and send notification of the changed
  // preferences.
  InitPrefStore(MANAGED_PLATFORM_STORE, new_managed_platform_pref_store);
  InitPrefStore(DEVICE_MANAGEMENT_STORE, new_device_management_pref_store);
  InitPrefStore(RECOMMENDED_STORE, new_recommended_pref_store);

  std::vector<std::string>::const_iterator current;
  for (current = changed_paths.begin();
       current != changed_paths.end();
       ++current) {
    pref_notifier_->OnPreferenceChanged(current->c_str());
  }
}

void PrefValueStore::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::POLICY_CHANGED)
    RefreshPolicyPrefs();
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
