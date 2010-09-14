// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_value_store.h"

#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"

// static
PrefValueStore* PrefValueStore::CreatePrefValueStore(
    const FilePath& pref_filename,
    Profile* profile,
    bool user_only) {
  using policy::ConfigurationPolicyPrefStore;
  ConfigurationPolicyPrefStore* managed = NULL;
  ExtensionPrefStore* extension = NULL;
  CommandLinePrefStore* command_line = NULL;
  ConfigurationPolicyPrefStore* recommended = NULL;

  JsonPrefStore* user = new JsonPrefStore(
      pref_filename,
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE));
  DefaultPrefStore* default_store = new DefaultPrefStore();

  if (!user_only) {
    managed = ConfigurationPolicyPrefStore::CreateManagedPolicyPrefStore();
    extension = new ExtensionPrefStore(profile, PrefNotifier::EXTENSION_STORE);
    command_line = new CommandLinePrefStore(CommandLine::ForCurrentProcess());
    recommended =
        ConfigurationPolicyPrefStore::CreateRecommendedPolicyPrefStore();
  }
  return new PrefValueStore(managed, extension, command_line, user,
                            recommended, default_store);
}

PrefValueStore::~PrefValueStore() {}

bool PrefValueStore::GetValue(const std::string& name,
                              Value** out_value) const {
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  for (size_t i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get() &&
        pref_stores_[i]->prefs()->Get(name.c_str(), out_value)) {
      return true;
    }
  }
  // No value found for the given preference name: set the return false.
  *out_value = NULL;
  return false;
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

bool PrefValueStore::PrefValueInManagedStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::MANAGED_STORE);
}

bool PrefValueStore::PrefValueInExtensionStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::EXTENSION_STORE);
}

bool PrefValueStore::PrefValueInUserStore(const char* name) const {
  return PrefValueInStore(name, PrefNotifier::USER_STORE);
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

bool PrefValueStore::PrefValueInStore(const char* name,
                                      PrefNotifier::PrefStoreType type) const {
  if (!pref_stores_[type].get()) {
    // No store of that type set, so this pref can't be in it.
    return false;
  }
  Value* tmp_value;
  return pref_stores_[type]->prefs()->Get(name, &tmp_value);
}

void PrefValueStore::RefreshPolicyPrefsCompletion(
    PrefStore* new_managed_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback* callback_pointer) {
  scoped_ptr<AfterRefreshCallback> callback(callback_pointer);
  DictionaryValue* managed_prefs_before(
      pref_stores_[PrefNotifier::MANAGED_STORE]->prefs());
  DictionaryValue* managed_prefs_after(new_managed_pref_store->prefs());
  DictionaryValue* recommended_prefs_before(
      pref_stores_[PrefNotifier::RECOMMENDED_STORE]->prefs());
  DictionaryValue* recommended_prefs_after(new_recommended_pref_store->prefs());

  std::vector<std::string> changed_managed_paths;
  managed_prefs_before->GetDifferingPaths(managed_prefs_after,
                                          &changed_managed_paths);

  std::vector<std::string> changed_recommended_paths;
  recommended_prefs_before->GetDifferingPaths(recommended_prefs_after,
                                              &changed_recommended_paths);

  std::vector<std::string> changed_paths(changed_managed_paths.size() +
                                         changed_recommended_paths.size());
  std::vector<std::string>::iterator last_insert =
      std::merge(changed_managed_paths.begin(),
                 changed_managed_paths.end(),
                 changed_recommended_paths.begin(),
                 changed_recommended_paths.end(),
                 changed_paths.begin());
  changed_paths.resize(last_insert - changed_paths.begin());

  pref_stores_[PrefNotifier::MANAGED_STORE].reset(new_managed_pref_store);
  pref_stores_[PrefNotifier::RECOMMENDED_STORE].reset(
      new_recommended_pref_store);
  callback->Run(changed_paths);
}

void PrefValueStore::RefreshPolicyPrefsOnFileThread(
    ChromeThread::ID calling_thread_id,
    PrefStore* new_managed_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback* callback_pointer) {
  scoped_ptr<AfterRefreshCallback> callback(callback_pointer);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  scoped_ptr<PrefStore> managed_pref_store(new_managed_pref_store);
  scoped_ptr<PrefStore> recommended_pref_store(new_recommended_pref_store);

  PrefStore::PrefReadError read_error = new_managed_pref_store->ReadPrefs();
  if (read_error != PrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "refresh of managed policy failed: PrefReadError = "
               << read_error;
    return;
  }

  read_error = new_recommended_pref_store->ReadPrefs();
  if (read_error != PrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "refresh of recommended policy failed: PrefReadError = "
               << read_error;
    return;
  }

  ChromeThread::PostTask(
      calling_thread_id, FROM_HERE,
      NewRunnableMethod(this,
                        &PrefValueStore::RefreshPolicyPrefsCompletion,
                        managed_pref_store.release(),
                        recommended_pref_store.release(),
                        callback.release()));
}

void PrefValueStore::RefreshPolicyPrefs(
    PrefStore* new_managed_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback* callback) {
  ChromeThread::ID current_thread_id;
  CHECK(ChromeThread::GetCurrentThreadIdentifier(&current_thread_id));
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &PrefValueStore::RefreshPolicyPrefsOnFileThread,
                        current_thread_id,
                        new_managed_pref_store,
                        new_recommended_pref_store,
                        callback));
}

PrefValueStore::PrefValueStore(PrefStore* managed_prefs,
                               PrefStore* extension_prefs,
                               PrefStore* command_line_prefs,
                               PrefStore* user_prefs,
                               PrefStore* recommended_prefs,
                               PrefStore* default_prefs) {
  // NULL default pref store is usually bad, but may be OK for some unit tests.
  if (!default_prefs)
    LOG(WARNING) << "default pref store is null";
  pref_stores_[PrefNotifier::MANAGED_STORE].reset(managed_prefs);
  pref_stores_[PrefNotifier::EXTENSION_STORE].reset(extension_prefs);
  pref_stores_[PrefNotifier::COMMAND_LINE_STORE].reset(command_line_prefs);
  pref_stores_[PrefNotifier::USER_STORE].reset(user_prefs);
  pref_stores_[PrefNotifier::RECOMMENDED_STORE].reset(recommended_prefs);
  pref_stores_[PrefNotifier::DEFAULT_STORE].reset(default_prefs);
}
