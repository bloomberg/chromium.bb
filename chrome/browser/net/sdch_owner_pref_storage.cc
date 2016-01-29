// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/sdch_owner_pref_storage.h"

#include "base/prefs/persistent_pref_store.h"
#include "base/values.h"

namespace chrome_browser_net {

namespace {

static const char kStorageKey[] = "SDCH";

}  // namespace

SdchOwnerPrefStorage::SdchOwnerPrefStorage(PersistentPrefStore* storage)
    : storage_(storage),
      storage_key_(kStorageKey),
      init_observer_(nullptr) {
}

SdchOwnerPrefStorage::~SdchOwnerPrefStorage() {
  if (init_observer_)
    storage_->RemoveObserver(this);
}

net::SdchOwner::PrefStorage::ReadError
SdchOwnerPrefStorage::GetReadError() const {
  PersistentPrefStore::PrefReadError error = storage_->GetReadError();

  DCHECK_NE(error,
            PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE);
  DCHECK_NE(error, PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);

  switch (error) {
    case PersistentPrefStore::PREF_READ_ERROR_NONE:
      return PERSISTENCE_FAILURE_NONE;

    case PersistentPrefStore::PREF_READ_ERROR_NO_FILE:
      return PERSISTENCE_FAILURE_REASON_NO_FILE;

    case PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE:
    case PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE:
    case PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER:
    case PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED:
    case PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT:
      return PERSISTENCE_FAILURE_REASON_READ_FAILED;

    case PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED:
    case PersistentPrefStore::PREF_READ_ERROR_FILE_NOT_SPECIFIED:
    case PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
    case PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM:
    default:
      // We don't expect these other failures given our usage of prefs.
      NOTREACHED();
      return PERSISTENCE_FAILURE_REASON_OTHER;
  }
}

bool SdchOwnerPrefStorage::GetValue(
      const base::DictionaryValue** result) const {
  const base::Value* result_value = nullptr;
  if (!storage_->GetValue(storage_key_, &result_value))
    return false;
  return result_value->GetAsDictionary(result);
}

bool SdchOwnerPrefStorage::GetMutableValue(base::DictionaryValue** result) {
  base::Value* result_value = nullptr;
  if (!storage_->GetMutableValue(storage_key_, &result_value))
    return false;
  return result_value->GetAsDictionary(result);
}

void SdchOwnerPrefStorage::SetValue(scoped_ptr<base::DictionaryValue> value) {
  storage_->SetValue(storage_key_, std::move(value),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void SdchOwnerPrefStorage::ReportValueChanged() {
  storage_->ReportValueChanged(storage_key_,
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

bool SdchOwnerPrefStorage::IsInitializationComplete() {
  return storage_->IsInitializationComplete();
}

void SdchOwnerPrefStorage::StartObservingInit(net::SdchOwner* observer) {
  DCHECK(!init_observer_);
  init_observer_ = observer;
  storage_->AddObserver(this);
}

void SdchOwnerPrefStorage::StopObservingInit() {
  DCHECK(init_observer_);
  init_observer_ = nullptr;
  storage_->RemoveObserver(this);
}

void SdchOwnerPrefStorage::OnPrefValueChanged(const std::string& key) {
}

void SdchOwnerPrefStorage::OnInitializationCompleted(bool succeeded) {
  DCHECK(init_observer_);
  init_observer_->OnPrefStorageInitializationComplete(succeeded);
}

}  // namespace chrome_browser_net
