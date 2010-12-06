// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/in_memory_pref_store.h"

#include "base/values.h"

InMemoryPrefStore::InMemoryPrefStore() : prefs_(new DictionaryValue()) {
}

InMemoryPrefStore::~InMemoryPrefStore() {
}

DictionaryValue* InMemoryPrefStore::prefs() const {
  return prefs_.get();
}

PrefStore::PrefReadError InMemoryPrefStore::ReadPrefs() {
  return PrefStore::PREF_READ_ERROR_NONE;
}
