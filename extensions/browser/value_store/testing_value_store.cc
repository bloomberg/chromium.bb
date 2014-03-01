// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/testing_value_store.h"

#include "base/logging.h"

namespace {

const char kGenericErrorMessage[] = "TestingValueStore configured to error";

}  // namespace

TestingValueStore::TestingValueStore()
    : read_count_(0), write_count_(0), error_code_(OK) {}

TestingValueStore::~TestingValueStore() {}

size_t TestingValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t TestingValueStore::GetBytesInUse(
    const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

size_t TestingValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED() << "Not implemented";
  return 0;
}

ValueStore::ReadResult TestingValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult TestingValueStore::Get(
    const std::vector<std::string>& keys) {
  read_count_++;
  if (error_code_ != OK)
    return MakeReadResult(TestingError());

  base::DictionaryValue* settings = new base::DictionaryValue();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    base::Value* value = NULL;
    if (storage_.GetWithoutPathExpansion(*it, &value)) {
      settings->SetWithoutPathExpansion(*it, value->DeepCopy());
    }
  }
  return MakeReadResult(make_scoped_ptr(settings));
}

ValueStore::ReadResult TestingValueStore::Get() {
  read_count_++;
  if (error_code_ != OK)
    return MakeReadResult(TestingError());
  return MakeReadResult(make_scoped_ptr(storage_.DeepCopy()));
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  base::DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(options, settings);
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options, const base::DictionaryValue& settings) {
  write_count_++;
  if (error_code_ != OK)
    return MakeWriteResult(TestingError());

  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  for (base::DictionaryValue::Iterator it(settings);
       !it.IsAtEnd(); it.Advance()) {
    base::Value* old_value = NULL;
    if (!storage_.GetWithoutPathExpansion(it.key(), &old_value) ||
        !old_value->Equals(&it.value())) {
      changes->push_back(
          ValueStoreChange(
              it.key(),
              old_value ? old_value->DeepCopy() : old_value,
              it.value().DeepCopy()));
      storage_.SetWithoutPathExpansion(it.key(), it.value().DeepCopy());
    }
  }
  return MakeWriteResult(changes.Pass());
}

ValueStore::WriteResult TestingValueStore::Remove(const std::string& key) {
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult TestingValueStore::Remove(
    const std::vector<std::string>& keys) {
  write_count_++;
  if (error_code_ != OK)
    return MakeWriteResult(TestingError());

  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    scoped_ptr<base::Value> old_value;
    if (storage_.RemoveWithoutPathExpansion(*it, &old_value)) {
      changes->push_back(ValueStoreChange(*it, old_value.release(), NULL));
    }
  }
  return MakeWriteResult(changes.Pass());
}

ValueStore::WriteResult TestingValueStore::Clear() {
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator it(storage_);
       !it.IsAtEnd(); it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}

bool TestingValueStore::Restore() {
  return true;
}

bool TestingValueStore::RestoreKey(const std::string& key) {
  return true;
}

scoped_ptr<ValueStore::Error> TestingValueStore::TestingError() {
  return make_scoped_ptr(new ValueStore::Error(
      error_code_, kGenericErrorMessage, scoped_ptr<std::string>()));
}
