// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_pref_service.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_value_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Do-nothing implementation for TestingPrefService.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

}  // namespace

template<>
TestingPrefServiceBase<PrefService>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    PrefRegistry* pref_registry,
    PrefNotifierImpl* pref_notifier)
    : PrefService(pref_notifier,
                  new PrefValueStore(
                      managed_prefs,
                      NULL,
                      NULL,
                      user_prefs,
                      recommended_prefs,
                      pref_registry->defaults(),
                      pref_notifier),
                  user_prefs,
                  pref_registry,
                  base::Bind(&HandleReadError),
                  false),
      managed_prefs_(managed_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {
}

template<>
TestingPrefServiceBase<PrefServiceSyncable>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    PrefRegistry* pref_registry,
    PrefNotifierImpl* pref_notifier)
    : PrefServiceSyncable(pref_notifier,
                          new PrefValueStore(
                              managed_prefs,
                              NULL,
                              NULL,
                              user_prefs,
                              recommended_prefs,
                              pref_registry->defaults(),
                              pref_notifier),
                          user_prefs,
                          static_cast<PrefRegistrySyncable*>(pref_registry),
                          base::Bind(&HandleReadError),
                          false),
      managed_prefs_(managed_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {
}

TestingPrefServiceSimple::TestingPrefServiceSimple()
    : TestingPrefServiceBase<PrefService>(
        new TestingPrefStore(),
        new TestingPrefStore(),
        new TestingPrefStore(),
        new PrefRegistrySimple(),
        new PrefNotifierImpl()) {
}

TestingPrefServiceSimple::~TestingPrefServiceSimple() {
}

PrefRegistrySimple* TestingPrefServiceSimple::registry() {
  return static_cast<PrefRegistrySimple*>(DeprecatedGetPrefRegistry());
}

TestingPrefServiceSyncable::TestingPrefServiceSyncable()
    : TestingPrefServiceBase<PrefServiceSyncable>(
        new TestingPrefStore(),
        new TestingPrefStore(),
        new TestingPrefStore(),
        new PrefRegistrySyncable(),
        new PrefNotifierImpl()) {
}

TestingPrefServiceSyncable::~TestingPrefServiceSyncable() {
}

PrefRegistrySyncable* TestingPrefServiceSyncable::registry() {
  return static_cast<PrefRegistrySyncable*>(DeprecatedGetPrefRegistry());
}

ScopedTestingLocalState::ScopedTestingLocalState(
    TestingBrowserProcess* browser_process)
    : browser_process_(browser_process) {
  chrome::RegisterLocalState(&local_state_,
                             local_state_.registry());
  EXPECT_FALSE(browser_process->local_state());
  browser_process->SetLocalState(&local_state_);
}

ScopedTestingLocalState::~ScopedTestingLocalState() {
  EXPECT_EQ(&local_state_, browser_process_->local_state());
  browser_process_->SetLocalState(NULL);
}
