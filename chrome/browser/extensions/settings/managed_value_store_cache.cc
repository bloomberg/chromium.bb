// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/managed_value_store_cache.h"

#include <set>

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/value_store/policy_value_store.h"
#include "chrome/browser/value_store/value_store_change.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

ManagedValueStoreCache::ManagedValueStoreCache(
    policy::PolicyService* policy_service,
    scoped_refptr<SettingsObserverList> observers)
    : policy_service_(policy_service),
      observers_(observers) {
  policy_service_->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
}

scoped_refptr<base::MessageLoopProxy>
    ManagedValueStoreCache::GetMessageLoop() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const policy::PolicyMap& policies = policy_service_->GetPolicies(
      policy::POLICY_DOMAIN_EXTENSIONS, extension->id());
  PolicyValueStore store(&policies);
  callback.Run(&store);
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // There's no real storage backing this cache, so this is a nop.
}

void ManagedValueStoreCache::OnPolicyUpdated(policy::PolicyDomain domain,
                                             const std::string& component_id,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ValueStoreChangeList changes;
  std::set<std::string> changed_keys;
  previous.GetDifferingKeys(current, &changed_keys);
  for (std::set<std::string>::iterator it = changed_keys.begin();
       it != changed_keys.end(); ++it) {
    // A policy might've changed only its scope or level, while the value
    // stayed the same. Events should be triggered only for mandatory values
    // that have changed.
    const policy::PolicyMap::Entry* old_entry = previous.Get(*it);
    const policy::PolicyMap::Entry* new_entry = current.Get(*it);
    scoped_ptr<base::Value> old_value;
    scoped_ptr<base::Value> new_value;
    if (old_entry && old_entry->level == policy::POLICY_LEVEL_MANDATORY)
      old_value.reset(old_entry->value->DeepCopy());
    if (new_entry && new_entry->level == policy::POLICY_LEVEL_MANDATORY)
      new_value.reset(new_entry->value->DeepCopy());
    if (!base::Value::Equals(old_value.get(), new_value.get())) {
      changes.push_back(
          ValueStoreChange(*it, old_value.release(), new_value.release()));
    }
  }
  observers_->Notify(
      &SettingsObserver::OnSettingsChanged,
      component_id,
      settings_namespace::MANAGED,
      ValueStoreChange::ToJson(changes));
}

}  // namespace extensions
