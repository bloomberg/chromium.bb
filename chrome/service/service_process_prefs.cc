// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process_prefs.h"

#include "base/message_loop_proxy.h"
#include "base/values.h"

ServiceProcessPrefs::ServiceProcessPrefs(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* task_runner)
    : prefs_(new JsonPrefStore(pref_filename, task_runner)) {
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
  if (!prefs_->GetValue(key, &value) || !value->GetAsString(&result))
    return default_value;

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
  if (!prefs_->GetValue(key, &value) || !value->GetAsBoolean(&result))
    return default_value;

  return result;
}

void ServiceProcessPrefs::SetBoolean(const std::string& key, bool value) {
  prefs_->SetValue(key, Value::CreateBooleanValue(value));
}

int ServiceProcessPrefs::GetInt(const std::string& key,
                                int default_value) const {
  const Value* value;
  int result = default_value;
  if (!prefs_->GetValue(key, &value) || !value->GetAsInteger(&result))
    return default_value;

  return result;
}

void ServiceProcessPrefs::SetInt(const std::string& key, int value) {
  prefs_->SetValue(key, Value::CreateIntegerValue(value));
}

const DictionaryValue* ServiceProcessPrefs::GetDictionary(
    const std::string& key) const {
  const Value* value;
  if (!prefs_->GetValue(key, &value) ||
      !value->IsType(Value::TYPE_DICTIONARY)) {
    return NULL;
  }

  return static_cast<const DictionaryValue*>(value);
}

const base::ListValue* ServiceProcessPrefs::GetList(
    const std::string& key) const {
  const Value* value;
  if (!prefs_->GetValue(key, &value) || !value->IsType(Value::TYPE_LIST))
    return NULL;

  return static_cast<const ListValue*>(value);
}

void ServiceProcessPrefs::SetValue(const std::string& key, base::Value* value) {
  prefs_->SetValue(key, value);
}

void ServiceProcessPrefs::RemovePref(const std::string& key) {
  prefs_->RemoveValue(key);
}

