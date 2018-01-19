// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable_factory.h"

#include <memory>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "components/sync_preferences/pref_service_syncable.h"

#if !defined(OS_IOS)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_service.h"  // nogncheck
#include "components/policy/core/common/policy_types.h"    // nogncheck
#endif

namespace sync_preferences {

PrefServiceSyncableFactory::PrefServiceSyncableFactory() {}

PrefServiceSyncableFactory::~PrefServiceSyncableFactory() {}

void PrefServiceSyncableFactory::SetManagedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
#if !defined(OS_IOS)
  set_managed_prefs(new policy::ConfigurationPolicyPrefStore(
      connector, service, connector->GetHandlerList(),
      policy::POLICY_LEVEL_MANDATORY));
#else
  NOTREACHED();
#endif
}

void PrefServiceSyncableFactory::SetRecommendedPolicies(
    policy::PolicyService* service,
    policy::BrowserPolicyConnector* connector) {
#if !defined(OS_IOS)
  set_recommended_prefs(new policy::ConfigurationPolicyPrefStore(
      connector, service, connector->GetHandlerList(),
      policy::POLICY_LEVEL_RECOMMENDED));
#else
  NOTREACHED();
#endif
}

void PrefServiceSyncableFactory::SetPrefModelAssociatorClient(
    PrefModelAssociatorClient* pref_model_associator_client) {
  pref_model_associator_client_ = pref_model_associator_client;
}

std::unique_ptr<PrefServiceSyncable> PrefServiceSyncableFactory::CreateSyncable(
    user_prefs::PrefRegistrySyncable* pref_registry,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  TRACE_EVENT0("browser", "PrefServiceSyncableFactory::CreateSyncable");
  auto pref_notifier = std::make_unique<PrefNotifierImpl>();
  auto pref_value_store = std::make_unique<PrefValueStore>(
      managed_prefs_.get(), supervised_user_prefs_.get(),
      extension_prefs_.get(), command_line_prefs_.get(), user_prefs_.get(),
      recommended_prefs_.get(), pref_registry->defaults().get(),
      pref_notifier.get(), std::move(delegate));
  return std::make_unique<PrefServiceSyncable>(
      std::move(pref_notifier), std::move(pref_value_store), user_prefs_.get(),
      pref_registry, pref_model_associator_client_, read_error_callback_,
      async_);
}

}  // namespace sync_preferences
