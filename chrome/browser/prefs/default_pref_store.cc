// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/default_pref_store.h"

#include "base/values.h"

DefaultPrefStore::DefaultPrefStore() : prefs_(new DictionaryValue()) {
}

DefaultPrefStore::~DefaultPrefStore() {
}

DictionaryValue* DefaultPrefStore::prefs() {
  return prefs_.get();
}

PrefStore::PrefReadError DefaultPrefStore::ReadPrefs() {
  return PrefStore::PREF_READ_ERROR_NONE;
}
