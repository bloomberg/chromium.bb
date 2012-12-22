// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"

#include "base/prefs/testing_pref_store.h"

PrefServiceMockBuilder::PrefServiceMockBuilder() {
  ResetDefaultState();
}

PrefServiceMockBuilder::~PrefServiceMockBuilder() {}

PrefServiceSimple* PrefServiceMockBuilder::CreateSimple() {
  PrefServiceSimple* service = PrefServiceBuilder::CreateSimple();
  return service;
}

PrefServiceSyncable* PrefServiceMockBuilder::CreateSyncable() {
  PrefServiceSyncable* service = PrefServiceSyncableBuilder::CreateSyncable();
  return service;
}

void PrefServiceMockBuilder::ResetDefaultState() {
  user_prefs_ = new TestingPrefStore;
}
