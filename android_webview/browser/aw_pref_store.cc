// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_pref_store.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

AwPrefStore::AwPrefStore() {}

AwPrefStore::~AwPrefStore() {}

bool AwPrefStore::GetValue(const std::string& key,
                           const base::Value** value) const {
  return prefs_.GetValue(key, value);
}

bool AwPrefStore::GetMutableValue(const std::string& key,
                                  base::Value** value) {
  return prefs_.GetValue(key, value);
}

void AwPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void AwPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool AwPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool AwPrefStore::IsInitializationComplete() const {
  return true;
}

void AwPrefStore::SetValue(const std::string& key,
                           scoped_ptr<base::Value> value,
                           uint32 flags) {
  DCHECK(value);
  if (prefs_.SetValue(key, value.Pass()))
    ReportValueChanged(key, flags);
}

void AwPrefStore::SetValueSilently(const std::string& key,
                                   scoped_ptr<base::Value> value,
                                   uint32 flags) {
  prefs_.SetValue(key, value.Pass());
}

void AwPrefStore::RemoveValue(const std::string& key, uint32 flags) {
  if (prefs_.RemoveValue(key))
    ReportValueChanged(key, flags);
}

bool AwPrefStore::ReadOnly() const {
  return false;
}

PersistentPrefStore::PrefReadError AwPrefStore::GetReadError() const {
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

PersistentPrefStore::PrefReadError AwPrefStore::ReadPrefs() {
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

void AwPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate_raw) {
}

void AwPrefStore::ReportValueChanged(const std::string& key, uint32 flags) {
  FOR_EACH_OBSERVER(Observer, observers_, OnPrefValueChanged(key));
}
