// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_value_store.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"

namespace {

// Returns true if the actual value is a valid type for the expected type when
// found in the given store.
bool IsValidType(Value::ValueType expected, Value::ValueType actual,
                 PrefNotifier::PrefStoreType store) {
  if (expected == actual)
    return true;

  // Dictionaries and lists are allowed to hold TYPE_NULL values too, but only
  // in the default pref store.
  if (store == PrefNotifier::DEFAULT_STORE &&
      actual == Value::TYPE_NULL &&
      (expected == Value::TYPE_DICTIONARY || expected == Value::TYPE_LIST)) {
    return true;
  }
  return false;
}

}  // namespace

// static
PrefValueStore* PrefValueStore::CreatePrefValueStore(
    const FilePath& pref_filename,
    Profile* profile,
    bool user_only) {
  using policy::ConfigurationPolicyPrefStore;
  ConfigurationPolicyPrefStore* managed = NULL;
  ConfigurationPolicyPrefStore* device_management = NULL;
  ExtensionPrefStore* extension = NULL;
  CommandLinePrefStore* command_line = NULL;
  ConfigurationPolicyPrefStore* recommended = NULL;

  JsonPrefStore* user = new JsonPrefStore(
      pref_filename,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  DefaultPrefStore* default_store = new DefaultPrefStore();

  if (!user_only) {
    managed =
        ConfigurationPolicyPrefStore::CreateManagedPlatformPolicyPrefStore();
    device_management =
        ConfigurationPolicyPrefStore::CreateDeviceManagementPolicyPrefStore();
    extension = new ExtensionPrefStore(profile, PrefNotifier::EXTENSION_STORE);
    command_line = new CommandLinePrefStore(CommandLine::ForCurrentProcess());
    recommended =
        ConfigurationPolicyPrefStore::CreateRecommendedPolicyPrefStore();
  }
  return new PrefValueStore(managed, device_management, extension,
                            command_line, user, recommended, default_store);
}

PrefValueStore::~PrefValueStore() {}

bool PrefValueStore::GetValue(const std::string& name,
                              Value** out_value) const {
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  for (size_t i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (GetValueFromStore(name.c_str(),
                          static_cast<PrefNotifier::PrefStoreType>(i),
                          out_value))
      return true;
  }
  return false;
}

bool PrefValueStore::GetUserValue(const std::string& name,
                                  Value** out_value) const {
  return GetValueFromStore(name.c_str(), PrefNotifier::USER_STORE, out_value);
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
  for (size_t i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get())
      success = pref_stores_[i]->WritePrefs() && success;
  }
  return success;
}

void PrefValueStore::ScheduleWritePrefs() {
  for (size_t i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get())
      pref_stores_[i]->ScheduleWritePrefs();
  }
}

PrefStore::PrefReadError PrefValueStore::ReadPrefs() {
  PrefStore::PrefReadError result = PrefStore::PREF_READ_ERROR_NONE;
  for (size_t i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get()) {
      PrefStore::PrefReadError this_error = pref_stores_[i]->ReadPrefs();
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

bool PrefValueStore::PrefHasChanged(const char* path,
                                    PrefNotifier::PrefStoreType new_store) {
  DCHECK(new_store != PrefNotifier::INVALID_STORE);
  // Replying that the pref has changed may cause problems, but it's the safer
  // choice.
  if (new_store == PrefNotifier::INVALID_STORE)
    return true;

  PrefNotifier::PrefStoreType controller = ControllingPrefStoreForPref(path);
  DCHECK(controller != PrefNotifier::INVALID_STORE);
  if (controller == PrefNotifier::INVALID_STORE)
    return true;

  // If the pref is controlled by a higher-priority store, its effective value
  // cannot have changed.
  if (controller < new_store)
    return false;

  // Otherwise, we take the pref store's word that something changed.
  return true;
}

// Note the |DictionaryValue| referenced by the |PrefStore| USER_STORE
// (returned by the method prefs()) takes the ownership of the Value referenced
// by in_value.
bool PrefValueStore::SetUserPrefValue(const char* name, Value* in_value) {
  Value* old_value = NULL;
  pref_stores_[PrefNotifier::USER_STORE]->prefs()->Get(name, &old_value);
  bool value_changed = !(old_value && old_value->Equals(in_value));

  pref_stores_[PrefNotifier::USER_STORE]->prefs()->Set(name, in_value);
  return value_changed;
}

// Note the |DictionaryValue| referenced by the |PrefStore| DEFAULT_STORE
// (returned by the method prefs()) takes the ownership of the Value referenced
// by in_value.
void PrefValueStore::SetDefaultPrefValue(const char* name, Value* in_value) {
  pref_stores_[PrefNotifier::DEFAULT_STORE]->prefs()->Set(name, in_value);
}

bool PrefValueStore::ReadOnly() {
  return pref_stores_[PrefNotifier::USER_STORE]->ReadOnly();
}

bool PrefValueStore::RemoveUserPrefValue(const char* name) {
  if (pref_stores_[PrefNotifier::USER_STORE].get()) {
    return pref_stores_[PrefNotifier::USER_STORE]->prefs()->Remove(name, NULL);
  }
  return false;
}

bool PrefValueStore::PrefValueInManagedPlatformStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::MANAGED_PLATFORM_STORE);
}

bool PrefValueStore::PrefValueInDeviceManagementStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::DEVICE_MANAGEMENT_STORE);
}

bool PrefValueStore::PrefValueInExtensionStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::EXTENSION_STORE);
}

bool PrefValueStore::PrefValueInUserStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::USER_STORE);
}

bool PrefValueStore::PrefValueInStoreRange(
    const char* name,
    PrefNotifier::PrefStoreType first_checked_store,
    PrefNotifier::PrefStoreType last_checked_store) {
  if (first_checked_store > last_checked_store) {
    NOTREACHED();
    return false;
  }

  for (size_t i = first_checked_store;
       i <= static_cast<size_t>(last_checked_store); ++i) {
    if (PrefValueInStore(name, static_cast<PrefNotifier::PrefStoreType>(i)))
      return true;
  }
  return false;
}

bool PrefValueStore::PrefValueFromExtensionStore(const char* name) const {
  return ControllingPrefStoreForPref(name) == PrefNotifier::EXTENSION_STORE;
}

bool PrefValueStore::PrefValueFromUserStore(const char* name) const {
  return ControllingPrefStoreForPref(name) == PrefNotifier::USER_STORE;
}

bool PrefValueStore::PrefValueFromDefaultStore(const char* name) const {
  return ControllingPrefStoreForPref(name) == PrefNotifier::DEFAULT_STORE;
}

bool PrefValueStore::PrefValueUserModifiable(const char* name) const {
  PrefNotifier::PrefStoreType effective_store =
      ControllingPrefStoreForPref(name);
  return effective_store >= PrefNotifier::USER_STORE ||
         effective_store == PrefNotifier::INVALID_STORE;
}

PrefNotifier::PrefStoreType PrefValueStore::ControllingPrefStoreForPref(
    const char* name) const {
  for (int i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (PrefValueInStore(name, static_cast<PrefNotifier::PrefStoreType>(i)))
      return static_cast<PrefNotifier::PrefStoreType>(i);
  }
  return PrefNotifier::INVALID_STORE;
}

bool PrefValueStore::PrefValueInStore(
    const char* name,
    PrefNotifier::PrefStoreType store) const {
  // Declare a temp Value* and call GetValueFromStore,
  // ignoring the output value.
  Value* tmp_value = NULL;
  return GetValueFromStore(name, store, &tmp_value);
}

bool PrefValueStore::GetValueFromStore(
    const char* name,
    PrefNotifier::PrefStoreType store,
    Value** out_value) const {
  // Only return true if we find a value and it is the correct type, so stale
  // values with the incorrect type will be ignored.
  if (pref_stores_[store].get() &&
      pref_stores_[store]->prefs()->Get(name, out_value)) {
    // If the value is the sentinel that redirects to the default
    // store, re-fetch the value from the default store explicitly.
    // Because the default values are not available when creating
    // stores, the default value must be fetched dynamically for every
    // redirect.
    if (PrefStore::IsUseDefaultSentinelValue(*out_value)) {
      DCHECK(pref_stores_[PrefNotifier::DEFAULT_STORE].get());
      if (!pref_stores_[PrefNotifier::DEFAULT_STORE]->prefs()->Get(name,
                                                                   out_value)) {
        *out_value = NULL;
        return false;
      }
      store = PrefNotifier::DEFAULT_STORE;
    }
    if (IsValidType(GetRegisteredType(name), (*out_value)->GetType(), store))
      return true;
  }
  // No valid value found for the given preference name: set the return false.
  *out_value = NULL;
  return false;
}

void PrefValueStore::RefreshPolicyPrefsOnFileThread(
    BrowserThread::ID calling_thread_id,
    PrefStore* new_managed_platform_pref_store,
    PrefStore* new_device_management_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback* callback_pointer) {
  scoped_ptr<AfterRefreshCallback> callback(callback_pointer);
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
                        recommended_pref_store.release(),
                        callback.release()));
}

void PrefValueStore::RefreshPolicyPrefsCompletion(
    PrefStore* new_managed_platform_pref_store,
    PrefStore* new_device_management_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback* callback_pointer) {
  scoped_ptr<AfterRefreshCallback> callback(callback_pointer);

  // Determine the paths of all the changed preferences values in the three
  // policy-related stores (managed platform, device management and
  // recommended).
  DictionaryValue* managed_platform_prefs_before(
      pref_stores_[PrefNotifier::MANAGED_PLATFORM_STORE]->prefs());
  DictionaryValue* managed_platform_prefs_after(
      new_managed_platform_pref_store->prefs());
  DictionaryValue* device_management_prefs_before(
      pref_stores_[PrefNotifier::DEVICE_MANAGEMENT_STORE]->prefs());
  DictionaryValue* device_management_prefs_after(
      new_device_management_pref_store->prefs());
  DictionaryValue* recommended_prefs_before(
      pref_stores_[PrefNotifier::RECOMMENDED_STORE]->prefs());
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
  pref_stores_[PrefNotifier::MANAGED_PLATFORM_STORE].reset(
      new_managed_platform_pref_store);
  pref_stores_[PrefNotifier::DEVICE_MANAGEMENT_STORE].reset(
      new_device_management_pref_store);
  pref_stores_[PrefNotifier::RECOMMENDED_STORE].reset(
      new_recommended_pref_store);
  callback->Run(changed_paths);
}

void PrefValueStore::RefreshPolicyPrefs(
    AfterRefreshCallback* callback) {
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
      ConfigurationPolicyPrefStore::CreateDeviceManagementPolicyPrefStore());
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
                        new_recommended_pref_store,
                        callback));
}

bool PrefValueStore::HasPolicyConflictingUserProxySettings() {
  using policy::ConfigurationPolicyPrefStore;
  ConfigurationPolicyPrefStore::ProxyPreferenceSet proxy_prefs;
  ConfigurationPolicyPrefStore::GetProxyPreferenceSet(&proxy_prefs);
  ConfigurationPolicyPrefStore::ProxyPreferenceSet::const_iterator i;
  for (i = proxy_prefs.begin(); i != proxy_prefs.end(); ++i) {
    if ((PrefValueInManagedPlatformStore(*i) ||
         PrefValueInDeviceManagementStore(*i)) &&
        PrefValueInStoreRange(*i,
                              PrefNotifier::COMMAND_LINE_STORE,
                              PrefNotifier::USER_STORE))
      return true;
  }
  return false;
}

PrefValueStore::PrefValueStore(PrefStore* managed_platform_prefs,
                               PrefStore* device_management_prefs,
                               PrefStore* extension_prefs,
                               PrefStore* command_line_prefs,
                               PrefStore* user_prefs,
                               PrefStore* recommended_prefs,
                               PrefStore* default_prefs) {
  // NULL default pref store is usually bad, but may be OK for some unit tests.
  if (!default_prefs)
    LOG(WARNING) << "default pref store is null";
  pref_stores_[PrefNotifier::MANAGED_PLATFORM_STORE].reset(
      managed_platform_prefs);
  pref_stores_[PrefNotifier::DEVICE_MANAGEMENT_STORE].reset(
      device_management_prefs);
  pref_stores_[PrefNotifier::EXTENSION_STORE].reset(extension_prefs);
  pref_stores_[PrefNotifier::COMMAND_LINE_STORE].reset(command_line_prefs);
  pref_stores_[PrefNotifier::USER_STORE].reset(user_prefs);
  pref_stores_[PrefNotifier::RECOMMENDED_STORE].reset(recommended_prefs);
  pref_stores_[PrefNotifier::DEFAULT_STORE].reset(default_prefs);
}
