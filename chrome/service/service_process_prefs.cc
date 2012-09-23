// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process_prefs.h"

#include "base/values.h"

ServiceProcessPrefs::ServiceProcessPrefs(
    const FilePath& pref_filename,
    base::MessageLoopProxy* file_message_loop_proxy)
    : prefs_(new JsonPrefStore(pref_filename, file_message_loop_proxy)) {
}

ServiceProcessPrefs::~ServiceProcessPrefs() {}

void ServiceProcessPrefs::ReadPrefs() {
  prefs_->ReadPrefs();
}

void ServiceProcessPrefs::WritePrefs() {
  prefs_->CommitPendingWrite();
}

std::string ServiceProcessPrefs::GetString(
    const std::string& key,
    const std::string& default_value) const {
  const Value* value;
  std::string result;
  if (prefs_->GetValue(key, &value) != PersistentPrefStore::READ_OK ||
      !value->GetAsString(&result)) {
    return default_value;
  }
  return result;
}

void ServiceProcessPrefs::SetString(const std::string& key,
                                    const std::string& value) {
  prefs_->SetValue(key, Value::CreateStringValue(value));
}

bool ServiceProcessPrefs::GetBoolean(const std::string& key,
                                     bool default_value) const {
  const Value* value;
  bool result = false;
  if (prefs_->GetValue(key, &value) != PersistentPrefStore::READ_OK ||
      !value->GetAsBoolean(&result)) {
    return default_value;
  }
  return result;
}

void ServiceProcessPrefs::SetBoolean(const std::string& key, bool value) {
  prefs_->SetValue(key, Value::CreateBooleanValue(value));
}

const DictionaryValue* ServiceProcessPrefs::GetDictionary(
    const std::string& key) const {
  const Value* value;
  if (prefs_->GetValue(key, &value) != PersistentPrefStore::READ_OK ||
      !value->IsType(Value::TYPE_DICTIONARY)) {
    return NULL;
  }

  return static_cast<const DictionaryValue*>(value);
}

const base::ListValue* ServiceProcessPrefs::GetList(
    const std::string& key) const {
  const Value* value;
  if (prefs_->GetValue(key, &value) != PersistentPrefStore::READ_OK ||
    !value->IsType(Value::TYPE_LIST)) {
      return NULL;
  }

  return static_cast<const ListValue*>(value);
}

void ServiceProcessPrefs::RemovePref(const std::string& key) {
  prefs_->RemoveValue(key);
}

