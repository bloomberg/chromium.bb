// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_value_map.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/values.h"

PrefValueMap::~PrefValueMap() {
  Clear();
}

bool PrefValueMap::GetValue(const std::string& key, Value** value) const {
  const Map::const_iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    if (value)
      *value = entry->second;
    return true;
  }

  return false;
}

bool PrefValueMap::SetValue(const std::string& key, Value* value) {
  DCHECK(value);
  scoped_ptr<Value> value_ptr(value);
  const Map::iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    if (Value::Equals(entry->second, value))
      return false;
    delete entry->second;
    entry->second = value_ptr.release();
  } else {
    prefs_[key] = value_ptr.release();
  }

  return true;
}

bool PrefValueMap::RemoveValue(const std::string& key) {
  const Map::iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    delete entry->second;
    prefs_.erase(entry);
    return true;
  }

  return false;
}

void PrefValueMap::Clear() {
  STLDeleteValues(&prefs_);
  prefs_.clear();
}
