// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PREF_SERVICE_SYNCABLE_H_
#define CHROME_TEST_BASE_TESTING_PREF_SERVICE_SYNCABLE_H_

#include "base/basictypes.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/prefs/pref_service_syncable.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Test version of PrefServiceSyncable.
class TestingPrefServiceSyncable
    : public TestingPrefServiceBase<PrefServiceSyncable,
                                    user_prefs::PrefRegistrySyncable> {
 public:
  TestingPrefServiceSyncable();
  TestingPrefServiceSyncable(TestingPrefStore* managed_prefs,
                             TestingPrefStore* user_prefs,
                             TestingPrefStore* recommended_prefs,
                             user_prefs::PrefRegistrySyncable* pref_registry,
                             PrefNotifierImpl* pref_notifier);
  virtual ~TestingPrefServiceSyncable();

  // This is provided as a convenience; on a production PrefService
  // you would do all registrations before constructing it, passing it
  // a PrefRegistry via its constructor (or via e.g. PrefServiceFactory).
  user_prefs::PrefRegistrySyncable* registry();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceSyncable);
};

template <>
TestingPrefServiceBase<PrefServiceSyncable, user_prefs::PrefRegistrySyncable>::
    TestingPrefServiceBase(TestingPrefStore* managed_prefs,
                           TestingPrefStore* user_prefs,
                           TestingPrefStore* recommended_prefs,
                           user_prefs::PrefRegistrySyncable* pref_registry,
                           PrefNotifierImpl* pref_notifier);

#endif  // CHROME_TEST_BASE_TESTING_PREF_SERVICE_SYNCABLE_H_

