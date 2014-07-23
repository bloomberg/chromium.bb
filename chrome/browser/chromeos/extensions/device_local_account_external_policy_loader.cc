// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "extensions/browser/pref_names.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

DeviceLocalAccountExternalPolicyLoader::
    DeviceLocalAccountExternalPolicyLoader(policy::CloudPolicyStore* store,
                                           const base::FilePath& cache_dir)
    : store_(store),
      cache_dir_(cache_dir) {
}

bool DeviceLocalAccountExternalPolicyLoader::IsCacheRunning() const {
  return external_cache_;
}

void DeviceLocalAccountExternalPolicyLoader::StartCache(
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner) {
  DCHECK(!external_cache_);
  store_->AddObserver(this);
  external_cache_.reset(new ExternalCache(
      cache_dir_,
      g_browser_process->system_request_context(),
      cache_task_runner,
      this,
      true  /* always_check_updates */,
      false  /* wait_for_cache_initialization */));

  if (store_->is_initialized())
    UpdateExtensionListFromStore();
}

void DeviceLocalAccountExternalPolicyLoader::StopCache(
    const base::Closure& callback) {
  if (external_cache_) {
    external_cache_->Shutdown(callback);
    external_cache_.reset();
    store_->RemoveObserver(this);
  }

  base::DictionaryValue empty_prefs;
  OnExtensionListsUpdated(&empty_prefs);
}

void DeviceLocalAccountExternalPolicyLoader::StartLoading() {
  if (prefs_)
    LoadFinished();
}

void DeviceLocalAccountExternalPolicyLoader::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  DCHECK(external_cache_);
  DCHECK_EQ(store_, store);
  UpdateExtensionListFromStore();
}

void DeviceLocalAccountExternalPolicyLoader::OnStoreError(
    policy::CloudPolicyStore* store) {
  DCHECK(external_cache_);
  DCHECK_EQ(store_, store);
}

void DeviceLocalAccountExternalPolicyLoader::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  DCHECK(external_cache_ || prefs->empty());
  prefs_.reset(prefs->DeepCopy());
  LoadFinished();
}

ExternalCache*
DeviceLocalAccountExternalPolicyLoader::GetExternalCacheForTesting() {
  return external_cache_.get();
}

DeviceLocalAccountExternalPolicyLoader::
    ~DeviceLocalAccountExternalPolicyLoader() {
  DCHECK(!external_cache_);
}

void DeviceLocalAccountExternalPolicyLoader::UpdateExtensionListFromStore() {
  scoped_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  const policy::PolicyMap& policy_map = store_->policy_map();
  extensions::ExtensionInstallForcelistPolicyHandler policy_handler;
  if (policy_handler.CheckPolicySettings(policy_map, NULL)) {
    PrefValueMap pref_value_map;
    policy_handler.ApplyPolicySettings(policy_map, &pref_value_map);
    const base::Value* value = NULL;
    const base::DictionaryValue* dict = NULL;
    if (pref_value_map.GetValue(extensions::pref_names::kInstallForceList,
                                &value) &&
        value->GetAsDictionary(&dict)) {
      prefs.reset(dict->DeepCopy());
    }
  }

  external_cache_->UpdateExtensionsList(prefs.Pass());
}

}  // namespace chromeos
