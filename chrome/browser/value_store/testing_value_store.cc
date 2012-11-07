// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/testing_value_store.h"

#include "base/logging.h"

namespace {

const char kGenericErrorMessage[] = "TestingValueStore configured to error";

ValueStore::ReadResult ReadResultError() {
  return ValueStore::MakeReadResult(kGenericErrorMessage);
}

ValueStore::WriteResult WriteResultError() {
  return ValueStore::MakeWriteResult(kGenericErrorMessage);
}

std::vector<std::string> CreateVector(const std::string& string) {
  std::vector<std::string> strings;
  strings.push_back(string);
  return strings;
}

}  // namespace

TestingValueStore::TestingValueStore()
    : read_count_(0), write_count_(0), fail_all_requests_(false) {}

TestingValueStore::~TestingValueStore() {}

void TestingValueStore::SetFailAllRequests(bool fail_all_requests) {
  fail_all_requests_ = fail_all_requests;
}

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
  return Get(CreateVector(key));
}

ValueStore::ReadResult TestingValueStore::Get(
    const std::vector<std::string>& keys) {
  read_count_++;
  if (fail_all_requests_) {
    return ReadResultError();
  }

  DictionaryValue* settings = new DictionaryValue();
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value* value = NULL;
    if (storage_.GetWithoutPathExpansion(*it, &value)) {
      settings->SetWithoutPathExpansion(*it, value->DeepCopy());
    }
  }
  return MakeReadResult(settings);
}

ValueStore::ReadResult TestingValueStore::Get() {
  read_count_++;
  if (fail_all_requests_) {
    return ReadResultError();
  }
  return MakeReadResult(storage_.DeepCopy());
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.DeepCopy());
  return Set(options, settings);
}

ValueStore::WriteResult TestingValueStore::Set(
    WriteOptions options, const DictionaryValue& settings) {
  write_count_++;
  if (fail_all_requests_) {
    return WriteResultError();
  }

  scoped_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  for (DictionaryValue::Iterator it(settings); it.HasNext(); it.Advance()) {
    Value* old_value = NULL;
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
  return MakeWriteResult(changes.release());
}

ValueStore::WriteResult TestingValueStore::Remove(const std::string& key) {
  return Remove(CreateVector(key));
}

ValueStore::WriteResult TestingValueStore::Remove(
    const std::vector<std::string>& keys) {
  write_count_++;
  if (fail_all_requests_) {
    return WriteResultError();
  }

  scoped_ptr<ValueStoreChangeList> changes(
      new ValueStoreChangeList());
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    Value* old_value = NULL;
    if (storage_.RemoveWithoutPathExpansion(*it, &old_value)) {
      changes->push_back(ValueStoreChange(*it, old_value, NULL));
    }
  }
  return MakeWriteResult(changes.release());
}

ValueStore::WriteResult TestingValueStore::Clear() {
  std::vector<std::string> keys;
  for (DictionaryValue::Iterator it(storage_); it.HasNext(); it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}
