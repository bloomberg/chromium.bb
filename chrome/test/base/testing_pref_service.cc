// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_pref_service.h"

#include "base/bind.h"
#include "base/prefs/default_pref_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Do-nothing implementation for TestingPrefService.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
}

}  // namespace

template<>
TestingPrefServiceBase<PrefServiceSimple>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    DefaultPrefStore* default_store,
    PrefNotifierImpl* pref_notifier)
    : PrefServiceSimple(pref_notifier,
                        new PrefValueStore(
                            managed_prefs,
                            NULL,
                            NULL,
                            user_prefs,
                            recommended_prefs,
                            default_store,
                            pref_notifier),
                        user_prefs,
                        default_store,
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
    DefaultPrefStore* default_store,
    PrefNotifierImpl* pref_notifier)
    : PrefServiceSyncable(pref_notifier,
                          new PrefValueStore(
                              managed_prefs,
                              NULL,
                              NULL,
                              user_prefs,
                              recommended_prefs,
                              default_store,
                              pref_notifier),
                          user_prefs,
                          default_store,
                          base::Bind(&HandleReadError),
                          false),
      managed_prefs_(managed_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {
}

TestingPrefServiceSimple::TestingPrefServiceSimple()
    : TestingPrefServiceBase<PrefServiceSimple>(new TestingPrefStore(),
                                                new TestingPrefStore(),
                                                new TestingPrefStore(),
                                                new DefaultPrefStore(),
                                                new PrefNotifierImpl()) {
}

TestingPrefServiceSimple::~TestingPrefServiceSimple() {
}

TestingPrefServiceSyncable::TestingPrefServiceSyncable()
    : TestingPrefServiceBase<PrefServiceSyncable>(new TestingPrefStore(),
                                                  new TestingPrefStore(),
                                                  new TestingPrefStore(),
                                                  new DefaultPrefStore(),
                                                  new PrefNotifierImpl()) {
}

TestingPrefServiceSyncable::~TestingPrefServiceSyncable() {
}

ScopedTestingLocalState::ScopedTestingLocalState(
    TestingBrowserProcess* browser_process)
    : browser_process_(browser_process) {
  chrome::RegisterLocalState(&local_state_);
  EXPECT_FALSE(browser_process->local_state());
  browser_process->SetLocalState(&local_state_);
}

ScopedTestingLocalState::~ScopedTestingLocalState() {
  EXPECT_EQ(&local_state_, browser_process_->local_state());
  browser_process_->SetLocalState(NULL);
}
