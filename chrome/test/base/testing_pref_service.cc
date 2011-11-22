// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_pref_service.h"

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

TestingPrefServiceBase::TestingPrefServiceBase(
    TestingPrefStore* managed_platform_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_platform_prefs,
    DefaultPrefStore* default_store,
    PrefModelAssociator* pref_sync_associator,
    PrefNotifierImpl* pref_notifier)
    : PrefService(pref_notifier,
                  new PrefValueStore(
                      managed_platform_prefs,
                      NULL,
                      NULL,
                      NULL,
                      user_prefs,
                      recommended_platform_prefs,
                      NULL,
                      default_store,
                      pref_sync_associator,
                      pref_notifier),
                  user_prefs,
                  default_store,
                  pref_sync_associator,
                  false),
      managed_platform_prefs_(managed_platform_prefs),
      user_prefs_(user_prefs),
      recommended_platform_prefs_(recommended_platform_prefs) {
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

const Value* TestingPrefServiceBase::GetRecommendedPref(
    const char* path) const {
  return GetPref(recommended_platform_prefs_, path);
}

void TestingPrefServiceBase::SetRecommendedPref(
    const char* path, Value* value) {
  SetPref(recommended_platform_prefs_, path, value);
}

void TestingPrefServiceBase::RemoveRecommendedPref(const char* path) {
  RemovePref(recommended_platform_prefs_, path);
}

const Value* TestingPrefServiceBase::GetPref(TestingPrefStore* pref_store,
                                             const char* path) const {
  const Value* res;
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
                             new TestingPrefStore(),
                             new DefaultPrefStore(),
                             new PrefModelAssociator(),
                             new PrefNotifierImpl()) {
}

TestingPrefService::~TestingPrefService() {
}

ScopedTestingLocalState::ScopedTestingLocalState(
    TestingBrowserProcess* browser_process)
    : browser_process_(browser_process) {
  browser::RegisterLocalState(&local_state_);
  EXPECT_FALSE(browser_process->local_state());
  browser_process->SetLocalState(&local_state_);
}

ScopedTestingLocalState::~ScopedTestingLocalState() {
  EXPECT_EQ(&local_state_, browser_process_->local_state());
  browser_process_->SetLocalState(NULL);
}
