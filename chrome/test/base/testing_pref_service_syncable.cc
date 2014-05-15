// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_pref_service_syncable.h"

#include "base/bind.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_value_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

template <>
TestingPrefServiceBase<PrefServiceSyncable, user_prefs::PrefRegistrySyncable>::
    TestingPrefServiceBase(TestingPrefStore* managed_prefs,
                           TestingPrefStore* user_prefs,
                           TestingPrefStore* recommended_prefs,
                           user_prefs::PrefRegistrySyncable* pref_registry,
                           PrefNotifierImpl* pref_notifier)
    : PrefServiceSyncable(
          pref_notifier,
          new PrefValueStore(managed_prefs,
                             NULL,  // supervised_user_prefs
                             NULL,  // extension_prefs
                             NULL,  // command_line_prefs
                             user_prefs,
                             recommended_prefs,
                             pref_registry->defaults().get(),
                             pref_notifier),
          user_prefs,
          pref_registry,
          base::Bind(&TestingPrefServiceBase<
              PrefServiceSyncable,
              user_prefs::PrefRegistrySyncable>::HandleReadError),
          false),
      managed_prefs_(managed_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {}

TestingPrefServiceSyncable::TestingPrefServiceSyncable()
    : TestingPrefServiceBase<PrefServiceSyncable,
                             user_prefs::PrefRegistrySyncable>(
        new TestingPrefStore(),
        new TestingPrefStore(),
        new TestingPrefStore(),
        new user_prefs::PrefRegistrySyncable(),
        new PrefNotifierImpl()) {
}

TestingPrefServiceSyncable::TestingPrefServiceSyncable(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    user_prefs::PrefRegistrySyncable* pref_registry,
    PrefNotifierImpl* pref_notifier)
    : TestingPrefServiceBase<PrefServiceSyncable,
                             user_prefs::PrefRegistrySyncable>(
        managed_prefs,
        user_prefs,
        recommended_prefs,
        pref_registry,
        pref_notifier) {
}

TestingPrefServiceSyncable::~TestingPrefServiceSyncable() {
}

user_prefs::PrefRegistrySyncable* TestingPrefServiceSyncable::registry() {
  return static_cast<user_prefs::PrefRegistrySyncable*>(
      DeprecatedGetPrefRegistry());
}
