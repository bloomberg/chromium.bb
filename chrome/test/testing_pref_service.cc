// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_pref_service.h"

#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/dummy_pref_store.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"

TestingPrefService::TestingPrefValueStore::TestingPrefValueStore(
    PrefStore* managed_platform_prefs,
    PrefStore* device_management_prefs,
    PrefStore* extension_prefs,
    PrefStore* command_line_prefs,
    PrefStore* user_prefs,
    PrefStore* recommended_prefs,
    PrefStore* default_prefs)
    : PrefValueStore(managed_platform_prefs, device_management_prefs,
                     extension_prefs, command_line_prefs,
                     user_prefs, recommended_prefs, default_prefs, NULL) {
}

// TODO(pamg): Instantiate no PrefStores by default. Allow callers to specify
// which they want, and expand usage of this class to more unit tests.
TestingPrefService::TestingPrefService()
    : PrefService(new TestingPrefValueStore(
        managed_platform_prefs_ = new DummyPrefStore(),
        device_management_prefs_ = new DummyPrefStore(),
        NULL,
        NULL,
        user_prefs_ = new DummyPrefStore(),
        NULL,
        default_prefs_ = new DummyPrefStore())) {
}

TestingPrefService::TestingPrefService(
    policy::ConfigurationPolicyProvider* managed_platform_provider,
    policy::ConfigurationPolicyProvider* device_management_provider,
    CommandLine* command_line)
    : PrefService(new TestingPrefValueStore(
        managed_platform_prefs_ = CreatePolicyPrefStoreFromProvider(
            managed_platform_provider),
        device_management_prefs_ =
            CreatePolicyPrefStoreFromProvider(device_management_provider),
        NULL,
        CreateCommandLinePrefStore(command_line),
        user_prefs_ = new DummyPrefStore(),
        NULL,
        default_prefs_ = new DummyPrefStore())) {
}

PrefStore* TestingPrefService::CreatePolicyPrefStoreFromProvider(
    policy::ConfigurationPolicyProvider* provider) {
  if (provider)
    return new policy::ConfigurationPolicyPrefStore(provider);
  return new DummyPrefStore();
}

PrefStore* TestingPrefService::CreateCommandLinePrefStore(
    CommandLine* command_line) {
  if (command_line)
    return new CommandLinePrefStore(command_line);
  return new DummyPrefStore();
}

const Value* TestingPrefService::GetManagedPref(const char* path) {
  return GetPref(managed_platform_prefs_, path);
}

void TestingPrefService::SetManagedPref(const char* path, Value* value) {
  SetPref(managed_platform_prefs_, path, value);
}

void TestingPrefService::RemoveManagedPref(const char* path) {
  RemovePref(managed_platform_prefs_, path);
}

void TestingPrefService::SetManagedPrefWithoutNotification(const char* path,
                                                           Value* value) {
  managed_platform_prefs_->prefs()->Set(path, value);
}

void TestingPrefService::RemoveManagedPrefWithoutNotification(
    const char* path) {
  managed_platform_prefs_->prefs()->Remove(path, NULL);
}

const Value* TestingPrefService::GetUserPref(const char* path) {
  return GetPref(user_prefs_, path);
}

void TestingPrefService::SetUserPref(const char* path, Value* value) {
  SetPref(user_prefs_, path, value);
}

void TestingPrefService::RemoveUserPref(const char* path) {
  RemovePref(user_prefs_, path);
}

const Value* TestingPrefService::GetPref(PrefStore* pref_store,
                                         const char* path) {
  Value* result;
  return pref_store->prefs()->Get(path, &result) ? result : NULL;
}

void TestingPrefService::SetPref(PrefStore* pref_store,
                                 const char* path,
                                 Value* value) {
  pref_store->prefs()->Set(path, value);
  pref_notifier()->FireObservers(path);
}

void TestingPrefService::RemovePref(PrefStore* pref_store,
                                    const char* path) {
  pref_store->prefs()->Remove(path, NULL);
  pref_notifier()->FireObservers(path);
}
