// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_value_store.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/command_line_pref_store.h"
#include "chrome/browser/configuration_policy_pref_store.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"

// static
PrefValueStore* PrefValueStore::CreatePrefValueStore(
    const FilePath& pref_filename,
    Profile* profile,
    bool user_only) {
  ConfigurationPolicyPrefStore* managed = NULL;
  ExtensionPrefStore* extension = NULL;
  CommandLinePrefStore* command_line = NULL;
  JsonPrefStore* user = NULL;
  ConfigurationPolicyPrefStore* recommended = NULL;

  if (!pref_filename.empty()) {
    user = new JsonPrefStore(
        pref_filename,
        ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE));
  }

  if (!user_only) {
    managed = ConfigurationPolicyPrefStore::CreateManagedPolicyPrefStore();
    extension = new ExtensionPrefStore(profile);
    command_line = new CommandLinePrefStore(CommandLine::ForCurrentProcess());
    recommended =
        ConfigurationPolicyPrefStore::CreateRecommendedPolicyPrefStore();
  }
  return new PrefValueStore(managed, extension, command_line, user,
      recommended);
}

PrefValueStore::~PrefValueStore() {}

bool PrefValueStore::GetValue(const std::wstring& name,
                              Value** out_value) const {
  // Check the |PrefStore|s in order of their priority from highest to lowest
  // to find the value of the preference described by the given preference name.
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
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
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get())
      success = pref_stores_[i]->WritePrefs() && success;
  }
  return success;
}

void PrefValueStore::ScheduleWritePrefs() {
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (pref_stores_[i].get())
      pref_stores_[i]->ScheduleWritePrefs();
  }
}

PrefStore::PrefReadError PrefValueStore::ReadPrefs() {
  PrefStore::PrefReadError result = PrefStore::PREF_READ_ERROR_NONE;
  for (size_t i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
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

bool PrefValueStore::HasPrefPath(const wchar_t* path) const {
  Value* tmp_value = NULL;
  const std::wstring name(path);
  bool rv = GetValue(name, &tmp_value);
  return rv;
}

// Note the |DictionaryValue| referenced by the |PrefStore| user_prefs_
// (returned by the method prefs()) takes the ownership of the Value referenced
// by in_value.
void PrefValueStore::SetUserPrefValue(const wchar_t* name, Value* in_value) {
  pref_stores_[USER]->prefs()->Set(name, in_value);
}

bool PrefValueStore::ReadOnly() {
  return pref_stores_[USER]->ReadOnly();
}

void PrefValueStore::RemoveUserPrefValue(const wchar_t* name) {
  if (pref_stores_[USER].get()) {
    pref_stores_[USER]->prefs()->Remove(name, NULL);
  }
}

bool PrefValueStore::PrefValueInManagedStore(const wchar_t* name) {
  return PrefValueInStore(name, MANAGED);
}

bool PrefValueStore::PrefValueInExtensionStore(const wchar_t* name) {
  return PrefValueInStore(name, EXTENSION);
}

bool PrefValueStore::PrefValueInUserStore(const wchar_t* name) {
  return PrefValueInStore(name, USER);
}

bool PrefValueStore::PrefValueFromExtensionStore(const wchar_t* name) {
  return ControllingPrefStoreForPref(name) == EXTENSION;
}

bool PrefValueStore::PrefValueFromUserStore(const wchar_t* name) {
  return ControllingPrefStoreForPref(name) == USER;
}

bool PrefValueStore::PrefValueUserModifiable(const wchar_t* name) {
  PrefStoreType effective_store = ControllingPrefStoreForPref(name);
  return effective_store >= USER || effective_store == INVALID;
}

bool PrefValueStore::PrefValueInStore(const wchar_t* name, PrefStoreType type) {
  if (!pref_stores_[type].get()) {
    // No store of that type set, so this pref can't be in it.
    return false;
  }
  Value* tmp_value;
  return pref_stores_[type]->prefs()->Get(name, &tmp_value);
}

PrefValueStore::PrefStoreType PrefValueStore::ControllingPrefStoreForPref(
    const wchar_t* name) {
  for (int i = 0; i <= PREF_STORE_TYPE_MAX; ++i) {
    if (PrefValueInStore(name, static_cast<PrefStoreType>(i)))
      return static_cast<PrefStoreType>(i);
  }
  return INVALID;
}

void PrefValueStore::RefreshPolicyPrefsCompletion(
    PrefStore* new_managed_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback callback) {

  DictionaryValue* managed_prefs_before(pref_stores_[MANAGED]->prefs());
  DictionaryValue* managed_prefs_after(new_managed_pref_store->prefs());
  DictionaryValue* recommended_prefs_before(pref_stores_[RECOMMENDED]->prefs());
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

  pref_stores_[MANAGED].reset(new_managed_pref_store);
  pref_stores_[RECOMMENDED].reset(new_recommended_pref_store);
  callback->Run(changed_paths);
}

void PrefValueStore::RefreshPolicyPrefsOnFileThread(
    ChromeThread::ID calling_thread_id,
    PrefStore* new_managed_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback callback) {
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
                        callback));
}

void PrefValueStore::RefreshPolicyPrefs(
    PrefStore* new_managed_pref_store,
    PrefStore* new_recommended_pref_store,
    AfterRefreshCallback callback) {
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
  pref_stores_[MANAGED].reset(managed_prefs);
  pref_stores_[EXTENSION].reset(extension_prefs);
  pref_stores_[COMMAND_LINE].reset(command_line_prefs);
  pref_stores_[USER].reset(user_prefs);
  pref_stores_[RECOMMENDED].reset(recommended_prefs);
}
