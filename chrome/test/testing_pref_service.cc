// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_pref_service.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"

TestingPrefServiceBase::TestingPrefServiceBase(
    TestingPrefStore* managed_platform_prefs,
    TestingPrefStore* device_management_prefs,
    TestingPrefStore* user_prefs)
    : PrefService(managed_platform_prefs,
                  device_management_prefs,
                  NULL,
                  NULL,
                  user_prefs,
                  NULL,
                  new DefaultPrefStore()),
      managed_platform_prefs_(managed_platform_prefs),
      device_management_prefs_(device_management_prefs),
      user_prefs_(user_prefs) {
}

TestingPrefServiceBase::~TestingPrefServiceBase() {
}

const Value* TestingPrefServiceBase::GetManagedPref(const char* path) const {
  return GetPref(managed_platform_prefs_, path);
}

void TestingPrefServiceBase::SetManagedPref(const char* path, Value* value) {
  SetPref(managed_platform_prefs_, path, value);
}

void TestingPrefServiceBase::RemoveManagedPref(const char* path) {
  RemovePref(managed_platform_prefs_, path);
}

const Value* TestingPrefServiceBase::GetUserPref(const char* path) const {
  return GetPref(user_prefs_, path);
}

void TestingPrefServiceBase::SetUserPref(const char* path, Value* value) {
  SetPref(user_prefs_, path, value);
}

void TestingPrefServiceBase::RemoveUserPref(const char* path) {
  RemovePref(user_prefs_, path);
}

const Value* TestingPrefServiceBase::GetPref(TestingPrefStore* pref_store,
                                             const char* path) const {
  Value* res;
  return pref_store->GetValue(path, &res) == PrefStore::READ_OK ? res : NULL;
}

void TestingPrefServiceBase::SetPref(TestingPrefStore* pref_store,
                                     const char* path,
                                     Value* value) {
  pref_store->SetValue(path, value);
}

void TestingPrefServiceBase::RemovePref(TestingPrefStore* pref_store,
                                        const char* path) {
  pref_store->RemoveValue(path);
}

TestingPrefService::TestingPrefService()
    : TestingPrefServiceBase(new TestingPrefStore(),
                             new TestingPrefStore(),
                             new TestingPrefStore()) {
}

TestingPrefService::~TestingPrefService() {
}
