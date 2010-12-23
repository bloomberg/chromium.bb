// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_pref_service.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"

// TODO(pamg): Instantiate no PrefStores by default. Allow callers to specify
// which they want, and expand usage of this class to more unit tests.
TestingPrefService::TestingPrefService()
    : PrefService(
        managed_platform_prefs_ = new TestingPrefStore(),
        device_management_prefs_ = new TestingPrefStore(),
        NULL,
        NULL,
        user_prefs_ = new TestingPrefStore(),
        NULL) {
}

const Value* TestingPrefService::GetManagedPref(const char* path) const {
  return GetPref(managed_platform_prefs_, path);
}

void TestingPrefService::SetManagedPref(const char* path, Value* value) {
  SetPref(managed_platform_prefs_, path, value);
}

void TestingPrefService::RemoveManagedPref(const char* path) {
  RemovePref(managed_platform_prefs_, path);
}

const Value* TestingPrefService::GetUserPref(const char* path) const {
  return GetPref(user_prefs_, path);
}

void TestingPrefService::SetUserPref(const char* path, Value* value) {
  SetPref(user_prefs_, path, value);
}

void TestingPrefService::RemoveUserPref(const char* path) {
  RemovePref(user_prefs_, path);
}

const Value* TestingPrefService::GetPref(TestingPrefStore* pref_store,
                                         const char* path) const {
  Value* res;
  return pref_store->GetValue(path, &res) == PrefStore::READ_OK ? res : NULL;
}

void TestingPrefService::SetPref(TestingPrefStore* pref_store,
                                 const char* path,
                                 Value* value) {
  pref_store->SetValue(path, value);
}

void TestingPrefService::RemovePref(TestingPrefStore* pref_store,
                                    const char* path) {
  pref_store->RemoveValue(path);
}
