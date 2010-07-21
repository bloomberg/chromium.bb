// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_pref_service.h"

#include "chrome/browser/dummy_pref_store.h"
#include "chrome/browser/pref_value_store.h"

TestingPrefService::TestingPrefService()
    : PrefService(new PrefValueStore(
          managed_prefs_ = new DummyPrefStore(),
          NULL,
          NULL,
          user_prefs_ = new DummyPrefStore(),
          NULL)) {
}

const Value* TestingPrefService::GetManagedPref(const wchar_t* path) {
  return GetPref(managed_prefs_, path);
}

void TestingPrefService::SetManagedPref(const wchar_t* path, Value* value) {
  SetPref(managed_prefs_, path, value);
}

void TestingPrefService::RemoveManagedPref(const wchar_t* path) {
  RemovePref(managed_prefs_, path);
}

const Value* TestingPrefService::GetUserPref(const wchar_t* path) {
  return GetPref(user_prefs_, path);
}

void TestingPrefService::SetUserPref(const wchar_t* path, Value* value) {
  SetPref(user_prefs_, path, value);
}

void TestingPrefService::RemoveUserPref(const wchar_t* path) {
  RemovePref(user_prefs_, path);
}

const Value* TestingPrefService::GetPref(PrefStore* pref_store,
                                         const wchar_t* path) {
  Value* result;
  return pref_store->prefs()->Get(path, &result) ? result : NULL;
}

void TestingPrefService::SetPref(PrefStore* pref_store,
                                 const wchar_t* path,
                                 Value* value) {
  pref_store->prefs()->Set(path, value);
  FireObservers(path);
}

void TestingPrefService::RemovePref(PrefStore* pref_store,
                                    const wchar_t* path) {
  pref_store->prefs()->Remove(path, NULL);
  FireObservers(path);
}
