// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_model_associator_client.h"

namespace {

const char kPreferencesFilename[] = "Preferences";

// Record PersistentPrefStore's reading errors distribution.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
  // Sample the histogram also for the successful case in order to get a
  // baseline on the success rate in addition to the error distribution.
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error,
                            PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);
}

void PrepareFactory(sync_preferences::PrefServiceSyncableFactory* factory,
                    const base::FilePath& pref_filename,
                    base::SequencedTaskRunner* pref_io_task_runner) {
  factory->set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      pref_filename, std::unique_ptr<PrefFilter>(), pref_io_task_runner));

  factory->set_read_error_callback(base::Bind(&HandleReadError));
  factory->SetPrefModelAssociatorClient(
      IOSChromePrefModelAssociatorClient::GetInstance());
}

}  // namespace

std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<PrefRegistry>& pref_registry) {
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(&factory, pref_filename, pref_io_task_runner);
  return factory.Create(pref_registry.get());
}

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateBrowserStatePrefs(
    const base::FilePath& browser_state_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry) {
  // chrome_prefs::CreateProfilePrefs uses ProfilePrefStoreManager to create
  // the preference store however since Chrome on iOS does not need to track
  // preference modifications (as applications are sand-boxed), it can use a
  // simple JsonPrefStore to store them (which is what PrefStoreManager uses
  // on platforms that do not track preference modifications).
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(&factory, browser_state_path.Append(kPreferencesFilename),
                 pref_io_task_runner);
  std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service =
      factory.CreateSyncable(pref_registry.get());
  return pref_service;
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoBrowserStatePrefs(
    sync_preferences::PrefServiceSyncable* pref_service) {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // browser state. All preferences that store information about the browsing
  // history or behaviour of the user should have this property.
  std::vector<const char*> overlay_pref_names;
  overlay_pref_names.push_back(proxy_config::prefs::kProxy);
  return pref_service->CreateIncognitoPrefService(
      /*incognito_extension_pref_store=*/nullptr, overlay_pref_names);
}
