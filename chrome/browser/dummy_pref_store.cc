// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dummy_pref_store.h"

#include "base/values.h"

DummyPrefStore::DummyPrefStore() : prefs_(new DictionaryValue()) { }

PrefStore::PrefReadError DummyPrefStore::ReadPrefs() {
  prefs_.reset(new DictionaryValue());
  return PrefStore::PREF_READ_ERROR_NONE;
}
