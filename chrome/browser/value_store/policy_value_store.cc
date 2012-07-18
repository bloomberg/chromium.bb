// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/policy_value_store.h"

#include "base/logging.h"

using base::DictionaryValue;
using base::Value;

namespace {

const char kReadOnlyStoreErrorMessage[] = "This is a read-only store.";

ValueStore::WriteResult WriteResultError() {
  return ValueStore::MakeWriteResult(kReadOnlyStoreErrorMessage);
}

}  // namespace

PolicyValueStore::PolicyValueStore(const policy::PolicyMap* policy_map)
    : policy_map_(policy_map) {}

PolicyValueStore::~PolicyValueStore() {}

size_t PolicyValueStore::GetBytesInUse(const std::string& key) {
  // This store doesn't use any real storage.
  return 0;
}

size_t PolicyValueStore::GetBytesInUse(const std::vector<std::string>& keys) {
  // This store doesn't use any real storage.
  return 0;
}

size_t PolicyValueStore::GetBytesInUse() {
  // This store doesn't use any real storage.
  return 0;
}

ValueStore::ReadResult PolicyValueStore::Get(const std::string& key) {
  DictionaryValue* result = new DictionaryValue();
  AddEntryIfMandatory(result, key, policy_map_->Get(key));
  return MakeReadResult(result);
}

ValueStore::ReadResult PolicyValueStore::Get(
    const std::vector<std::string>& keys) {
  DictionaryValue* result = new DictionaryValue();
  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    AddEntryIfMandatory(result, *it, policy_map_->Get(*it));
  }
  return MakeReadResult(result);
}

ValueStore::ReadResult PolicyValueStore::Get() {
  DictionaryValue* result = new DictionaryValue();
  for (policy::PolicyMap::const_iterator it = policy_map_->begin();
       it != policy_map_->end(); ++it) {
    AddEntryIfMandatory(result, it->first, &it->second);
  }
  return MakeReadResult(result);
}

ValueStore::WriteResult PolicyValueStore::Set(
    WriteOptions options, const std::string& key, const Value& value) {
  return WriteResultError();
}

ValueStore::WriteResult PolicyValueStore::Set(
    WriteOptions options, const DictionaryValue& settings) {
  return WriteResultError();
}

ValueStore::WriteResult PolicyValueStore::Remove(const std::string& key) {
  return WriteResultError();
}

ValueStore::WriteResult PolicyValueStore::Remove(
    const std::vector<std::string>& keys) {
  return WriteResultError();
}

ValueStore::WriteResult PolicyValueStore::Clear() {
  return WriteResultError();
}

void PolicyValueStore::AddEntryIfMandatory(
    DictionaryValue* result,
    const std::string& key,
    const policy::PolicyMap::Entry* entry) {
  // TODO(joaodasilva): only mandatory policies are exposed for now.
  // Add support for recommended policies later. http://crbug.com/108992
  if (!entry || entry->level != policy::POLICY_LEVEL_MANDATORY)
    return;
  result->SetWithoutPathExpansion(key, entry->value->DeepCopy());
}
