// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_value_store.h"

#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
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
  JsonPrefStore* user = NULL;
  ConfigurationPolicyPrefStore* recommended = NULL;

  user = new JsonPrefStore(
      pref_filename,
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE));

  if (!user_only) {
    managed = ConfigurationPolicyPrefStore::CreateManagedPolicyPrefStore();
    extension = new ExtensionPrefStore(profile, PrefNotifier::EXTENSION_STORE);
    command_line = new CommandLinePrefStore(CommandLine::ForCurrentProcess());
    recommended =
        ConfigurationPolicyPrefStore::CreateRecommendedPolicyPrefStore();
  }
  return new PrefValueStore(managed, extension, command_line, user,
      recommended);
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
  // No value found for the given preference name, set the return false.
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
  return rv;
}

bool PrefValueStore::PrefHasChanged(const char* path,
                                    PrefNotifier::PrefStoreType new_store,
                                    const Value* old_value) {
  DCHECK(new_store != PrefNotifier::INVALID_STORE);
  // Replying that the pref has changed may cause extra work, but it should
  // not be actively harmful.
  if (new_store == PrefNotifier::INVALID_STORE)
    return true;

  Value* new_value = NULL;
  GetValue(path, &new_value);
  // Some unit tests have no values for certain prefs.
  if (!new_value || !old_value->Equals(new_value))
    return true;

  // If there's a value in a store with lower priority than the |new_store|,
  // and no value in a store with higher priority, assume the |new_store| just
  // took control of the pref. (This assumption is wrong if the new value
  // and store are both the same as the old one, but that situation should be
  // rare, and reporting a change when none happened should not be harmful.)
  if (PrefValueInStoreRange(path, new_store, false) &&
      !PrefValueInStoreRange(path, new_store, true))
    return true;

  return false;
}

// Note the |DictionaryValue| referenced by the |PrefStore| user_prefs_
// (returned by the method prefs()) takes the ownership of the Value referenced
// by in_value.
void PrefValueStore::SetUserPrefValue(const char* name, Value* in_value) {
  pref_stores_[PrefNotifier::USER_STORE]->prefs()->Set(name, in_value);
}

bool PrefValueStore::ReadOnly() {
  return pref_stores_[PrefNotifier::USER_STORE]->ReadOnly();
}

void PrefValueStore::RemoveUserPrefValue(const char* name) {
  if (pref_stores_[PrefNotifier::USER_STORE].get()) {
    pref_stores_[PrefNotifier::USER_STORE]->prefs()->Remove(name, NULL);
  }
}

bool PrefValueStore::PrefValueInManagedStore(const char* name) {
  return PrefValueInStore(name, PrefNotifier::MANAGED_STORE);
}

bool PrefValueStore::PrefValueInExtensionStore(const char* name) {
  return PrefValueInStore(name, PrefNotifier::EXTENSION_STORE);
}

bool PrefValueStore::PrefValueInUserStore(const char* name) {
  return PrefValueInStore(name, PrefNotifier::USER_STORE);
}

bool PrefValueStore::PrefValueFromExtensionStore(const char* name) {
  return ControllingPrefStoreForPref(name) == PrefNotifier::EXTENSION_STORE;
}

bool PrefValueStore::PrefValueFromUserStore(const char* name) {
  return ControllingPrefStoreForPref(name) == PrefNotifier::USER_STORE;
}

bool PrefValueStore::PrefValueUserModifiable(const char* name) {
  PrefNotifier::PrefStoreType effective_store =
      ControllingPrefStoreForPref(name);
  return effective_store >= PrefNotifier::USER_STORE ||
         effective_store == PrefNotifier::INVALID_STORE;
}

bool PrefValueStore::PrefValueInStore(const char* name,
                                      PrefNotifier::PrefStoreType type) {
  if (!pref_stores_[type].get()) {
    // No store of that type set, so this pref can't be in it.
    return false;
  }
  Value* tmp_value;
  return pref_stores_[type]->prefs()->Get(name, &tmp_value);
}

bool PrefValueStore::PrefValueInStoreRange(const char* name,
                                           PrefNotifier::PrefStoreType boundary,
                                           bool higher_priority) {
  // Higher priorities are lower PrefStoreType values. The range is
  // non-inclusive of the boundary.
  int start = higher_priority ? 0 : boundary + 1;
  int end = higher_priority ? boundary - 1 :
      PrefNotifier::PREF_STORE_TYPE_MAX;

  for (int i = start; i <= end ; ++i) {
    if (PrefValueInStore(name, static_cast<PrefNotifier::PrefStoreType>(i)))
      return true;
  }
  return false;
}


PrefNotifier::PrefStoreType PrefValueStore::ControllingPrefStoreForPref(
    const char* name) {
  for (int i = 0; i <= PrefNotifier::PREF_STORE_TYPE_MAX; ++i) {
    if (PrefValueInStore(name, static_cast<PrefNotifier::PrefStoreType>(i)))
      return static_cast<PrefNotifier::PrefStoreType>(i);
  }
  return PrefNotifier::INVALID_STORE;
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
                               PrefStore* recommended_prefs) {
  pref_stores_[PrefNotifier::MANAGED_STORE].reset(managed_prefs);
  pref_stores_[PrefNotifier::EXTENSION_STORE].reset(extension_prefs);
  pref_stores_[PrefNotifier::COMMAND_LINE_STORE].reset(command_line_prefs);
  pref_stores_[PrefNotifier::USER_STORE].reset(user_prefs);
  pref_stores_[PrefNotifier::RECOMMENDED_STORE].reset(recommended_prefs);
}
