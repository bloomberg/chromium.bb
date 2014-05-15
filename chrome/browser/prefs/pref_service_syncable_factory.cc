// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_factory.h"

#include "base/debug/trace_event.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_value_store.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#endif

PrefServiceSyncableFactory::PrefServiceSyncableFactory() {
}

PrefServiceSyncableFactory::~PrefServiceSyncableFactory() {
}

#if defined(ENABLE_CONFIGURATION_POLICY)
void PrefServiceSyncableFactory::SetManagedPolicies(
    policy::PolicyService* service) {
  set_managed_prefs(
      new policy::ConfigurationPolicyPrefStore(
          service,
          g_browser_process->browser_policy_connector()->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY));
}

void PrefServiceSyncableFactory::SetRecommendedPolicies(
    policy::PolicyService* service) {
  set_recommended_prefs(
      new policy::ConfigurationPolicyPrefStore(
          service,
          g_browser_process->browser_policy_connector()->GetHandlerList(),
          policy::POLICY_LEVEL_RECOMMENDED));
}
#endif

void PrefServiceSyncableFactory::SetCommandLine(CommandLine* command_line) {
  set_command_line_prefs(new CommandLinePrefStore(command_line));
}

scoped_ptr<PrefServiceSyncable> PrefServiceSyncableFactory::CreateSyncable(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  TRACE_EVENT0("browser", "PrefServiceSyncableFactory::CreateSyncable");
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  scoped_ptr<PrefServiceSyncable> pref_service(
      new PrefServiceSyncable(
          pref_notifier,
          new PrefValueStore(managed_prefs_.get(),
                             supervised_user_prefs_.get(),
                             extension_prefs_.get(),
                             command_line_prefs_.get(),
                             user_prefs_.get(),
                             recommended_prefs_.get(),
                             pref_registry->defaults().get(),
                             pref_notifier),
          user_prefs_.get(),
          pref_registry,
          read_error_callback_,
          async_));
  return pref_service.Pass();
}
