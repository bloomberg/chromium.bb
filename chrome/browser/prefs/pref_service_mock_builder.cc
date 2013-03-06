// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"

#include "base/prefs/testing_pref_store.h"
#include "components/user_prefs/pref_registry_syncable.h"

PrefServiceMockBuilder::PrefServiceMockBuilder() {
  ResetDefaultState();
}

PrefServiceMockBuilder::~PrefServiceMockBuilder() {}

PrefService* PrefServiceMockBuilder::Create(PrefRegistry* pref_registry) {
  PrefService* service = PrefServiceBuilder::Create(pref_registry);
  return service;
}

PrefServiceSyncable* PrefServiceMockBuilder::CreateSyncable(
    PrefRegistrySyncable* pref_registry) {
  PrefServiceSyncable* service =
      PrefServiceSyncableBuilder::CreateSyncable(pref_registry);
  return service;
}

void PrefServiceMockBuilder::ResetDefaultState() {
  user_prefs_ = new TestingPrefStore;
}
