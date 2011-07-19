// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/default_pref_store.h"

DefaultPrefStore::DefaultPrefStore() {}

DefaultPrefStore::~DefaultPrefStore() {}

void DefaultPrefStore::SetDefaultValue(const std::string& key, Value* value) {
  CHECK(GetValue(key, NULL) == READ_NO_VALUE);
  SetValue(key, value);
}

Value::ValueType DefaultPrefStore::GetType(const std::string& key) const {
  const Value* value;
  return GetValue(key, &value) == READ_OK ? value->GetType()
                                          : Value::TYPE_NULL;
}
