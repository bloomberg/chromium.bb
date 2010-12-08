// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/testing_pref_store.h"

#include "base/values.h"

TestingPrefStore::TestingPrefStore()
    : prefs_(new DictionaryValue()),
      read_only_(true),
      prefs_written_(false),
      init_complete_(false) { }

PrefStore::PrefReadError TestingPrefStore::ReadPrefs() {
  prefs_.reset(new DictionaryValue());
  return PrefStore::PREF_READ_ERROR_NONE;
}

bool TestingPrefStore::WritePrefs() {
  prefs_written_ = true;
  return prefs_written_;
}

void TestingPrefStore::NotifyPrefValueChanged(const std::string& key) {
  PrefStoreBase::NotifyPrefValueChanged(key);
}

void TestingPrefStore::NotifyInitializationCompleted() {
  PrefStoreBase::NotifyInitializationCompleted();
}

void TestingPrefStore::SetInitializationCompleted() {
  init_complete_ = true;
  NotifyInitializationCompleted();
}
