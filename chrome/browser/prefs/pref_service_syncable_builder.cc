// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_builder.h"

#include "base/debug/trace_event.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_value_store.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "components/user_prefs/pref_registry_syncable.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/policy_types.h"
#endif

PrefServiceSyncableBuilder::PrefServiceSyncableBuilder() {
}

PrefServiceSyncableBuilder::~PrefServiceSyncableBuilder() {
}

#if defined(ENABLE_CONFIGURATION_POLICY)
PrefServiceSyncableBuilder& PrefServiceSyncableBuilder::WithManagedPolicies(
    policy::PolicyService* service) {
  WithManagedPrefs(new policy::ConfigurationPolicyPrefStore(
      service,
      g_browser_process->browser_policy_connector()->GetHandlerList(),
      policy::POLICY_LEVEL_MANDATORY));
  return *this;
}

PrefServiceSyncableBuilder& PrefServiceSyncableBuilder::WithRecommendedPolicies(
    policy::PolicyService* service) {
  WithRecommendedPrefs(new policy::ConfigurationPolicyPrefStore(
      service,
      g_browser_process->browser_policy_connector()->GetHandlerList(),
      policy::POLICY_LEVEL_RECOMMENDED));
  return *this;
}
#endif

PrefServiceSyncableBuilder&
PrefServiceSyncableBuilder::WithCommandLine(CommandLine* command_line) {
  WithCommandLinePrefs(new CommandLinePrefStore(command_line));
  return *this;
}

PrefServiceSyncable* PrefServiceSyncableBuilder::CreateSyncable(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  TRACE_EVENT0("browser", "PrefServiceSyncableBuilder::CreateSyncable");
  PrefNotifierImpl* pref_notifier = new PrefNotifierImpl();
  PrefServiceSyncable* pref_service = new PrefServiceSyncable(
      pref_notifier,
      new PrefValueStore(managed_prefs_.get(),
                         managed_user_prefs_.get(),
                         extension_prefs_.get(),
                         command_line_prefs_.get(),
                         user_prefs_.get(),
                         recommended_prefs_.get(),
                         pref_registry->defaults().get(),
                         pref_notifier),
      user_prefs_.get(),
      pref_registry,
      read_error_callback_,
      async_);
  ResetDefaultState();
  return pref_service;
}
