// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/overlay_persistent_pref_store.h"

OverlayPersistentPrefStore::OverlayPersistentPrefStore(
    PersistentPrefStore* underlay)
    : underlay_(underlay) {
  underlay_->AddObserver(this);
}
OverlayPersistentPrefStore::~OverlayPersistentPrefStore() {
  underlay_->RemoveObserver(this);
}

void OverlayPersistentPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void OverlayPersistentPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool OverlayPersistentPrefStore::IsInitializationComplete() const {
  return underlay_->IsInitializationComplete();
}

PrefStore::ReadResult OverlayPersistentPrefStore::GetValue(
    const std::string& key, Value** result) const {
  if (overlay_.GetValue(key, result))
    return READ_OK;
  return underlay_->GetValue(key, result);
}

void OverlayPersistentPrefStore::SetValue(const std::string& key,
                                          Value* value) {
  if (overlay_.SetValue(key, value))
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

void OverlayPersistentPrefStore::SetValueSilently(const std::string& key,
                                                  Value* value) {
  overlay_.SetValue(key, value);
}

void OverlayPersistentPrefStore::RemoveValue(const std::string& key) {
  if (overlay_.RemoveValue(key))
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

bool OverlayPersistentPrefStore::ReadOnly() const {
  return false;
}

PersistentPrefStore::PrefReadError OverlayPersistentPrefStore::ReadPrefs() {
  // We do not read intentionally.
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

bool OverlayPersistentPrefStore::WritePrefs() {
  // We do not write intentionally.
  return true;
}

void OverlayPersistentPrefStore::ScheduleWritePrefs() {
  // We do not write intentionally.
}

void OverlayPersistentPrefStore::ReportValueChanged(const std::string& key) {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

void OverlayPersistentPrefStore::OnPrefValueChanged(const std::string& key) {
  if (!overlay_.GetValue(key, NULL))
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

void OverlayPersistentPrefStore::OnInitializationCompleted() {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                    OnInitializationCompleted());
}
