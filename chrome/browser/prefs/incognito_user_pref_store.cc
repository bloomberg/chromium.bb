// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_user_pref_store.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"

IncognitoUserPrefStore::IncognitoUserPrefStore(
    PersistentPrefStore* underlay)
    : underlay_(underlay) {
  underlay_->AddObserver(this);
}

IncognitoUserPrefStore::~IncognitoUserPrefStore() {
  underlay_->RemoveObserver(this);
}

bool IncognitoUserPrefStore::IsSetInOverlay(const std::string& key) const {
  return overlay_.GetValue(key, NULL);
}

void IncognitoUserPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void IncognitoUserPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool IncognitoUserPrefStore::IsInitializationComplete() const {
  return underlay_->IsInitializationComplete();
}

PrefStore::ReadResult IncognitoUserPrefStore::GetValue(
    const std::string& key,
    const Value** result) const {
  // If the |key| shall NOT be stored in the overlay store, there must not
  // be an entry.
  DCHECK(ShallBeStoredInOverlay(key) || !overlay_.GetValue(key, NULL));

  if (overlay_.GetValue(key, result))
    return READ_OK;
  return underlay_->GetValue(key, result);
}

PrefStore::ReadResult IncognitoUserPrefStore::GetMutableValue(
    const std::string& key,
    Value** result) {
  if (!ShallBeStoredInOverlay(key))
    return underlay_->GetMutableValue(key, result);

  if (overlay_.GetValue(key, result))
    return READ_OK;

  // Try to create copy of underlay if the overlay does not contain a value.
  Value* underlay_value = NULL;
  PrefStore::ReadResult read_result =
      underlay_->GetMutableValue(key, &underlay_value);
  if (read_result != READ_OK)
    return read_result;

  *result = underlay_value->DeepCopy();
  overlay_.SetValue(key, *result);
  return READ_OK;
}

void IncognitoUserPrefStore::SetValue(const std::string& key,
                                      Value* value) {
  if (!ShallBeStoredInOverlay(key)) {
    underlay_->SetValue(key, value);
    return;
  }

  if (overlay_.SetValue(key, value))
    ReportValueChanged(key);
}

void IncognitoUserPrefStore::SetValueSilently(const std::string& key,
                                              Value* value) {
  if (!ShallBeStoredInOverlay(key)) {
    underlay_->SetValueSilently(key, value);
    return;
  }

  overlay_.SetValue(key, value);
}

void IncognitoUserPrefStore::RemoveValue(const std::string& key) {
  if (!ShallBeStoredInOverlay(key)) {
    underlay_->RemoveValue(key);
    return;
  }

  if (overlay_.RemoveValue(key))
    ReportValueChanged(key);
}

bool IncognitoUserPrefStore::ReadOnly() const {
  return false;
}

PersistentPrefStore::PrefReadError IncognitoUserPrefStore::ReadPrefs() {
  // We do not read intentionally.
  OnInitializationCompleted(true);
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

void IncognitoUserPrefStore::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate_raw) {
  scoped_ptr<ReadErrorDelegate> error_delegate(error_delegate_raw);
  // We do not read intentionally.
  OnInitializationCompleted(true);
}

bool IncognitoUserPrefStore::WritePrefs() {
  // We do not write our content intentionally.
  return true;
}

void IncognitoUserPrefStore::ScheduleWritePrefs() {
  underlay_->ScheduleWritePrefs();
  // We do not write our content intentionally.
}

void IncognitoUserPrefStore::CommitPendingWrite() {
  underlay_->CommitPendingWrite();
  // We do not write our content intentionally.
}

void IncognitoUserPrefStore::ReportValueChanged(const std::string& key) {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

void IncognitoUserPrefStore::CheckIfValueDestroyed(const std::string& key) {
}

void IncognitoUserPrefStore::OnPrefValueChanged(const std::string& key) {
  if (!overlay_.GetValue(key, NULL))
    ReportValueChanged(key);
}

void IncognitoUserPrefStore::OnInitializationCompleted(bool succeeded) {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                    OnInitializationCompleted(succeeded));
}

bool IncognitoUserPrefStore::ShallBeStoredInOverlay(
    const std::string& key) const {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // profile:
  return key == prefs::kBrowserWindowPlacement;
}
