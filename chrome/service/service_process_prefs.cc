// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process_prefs.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/prefs/pref_filter.h"

ServiceProcessPrefs::ServiceProcessPrefs(const base::FilePath& pref_filename,
                                         base::SequencedTaskRunner* task_runner)
    : prefs_(new JsonPrefStore(pref_filename,
                               std::unique_ptr<PrefFilter>(),
                               task_runner)) {}

ServiceProcessPrefs::~ServiceProcessPrefs() {}

void ServiceProcessPrefs::ReadPrefs() {
  prefs_->ReadPrefs();
}

void ServiceProcessPrefs::WritePrefs() {
  prefs_->CommitPendingWrite(base::OnceClosure());
}

std::string ServiceProcessPrefs::GetString(
    const std::string& key,
    const std::string& default_value) const {
  const base::Value* value;
  std::string result;
  if (!prefs_->GetValue(key, &value) || !value->GetAsString(&result))
    return default_value;

  return result;
}

void ServiceProcessPrefs::SetString(const std::string& key,
                                    const std::string& value) {
  prefs_->SetValue(key, std::make_unique<base::Value>(value),
                   WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

bool ServiceProcessPrefs::GetBoolean(const std::string& key,
                                     bool default_value) const {
  const base::Value* value;
  bool result = false;
  if (!prefs_->GetValue(key, &value) || !value->GetAsBoolean(&result))
    return default_value;

  return result;
}

void ServiceProcessPrefs::SetBoolean(const std::string& key, bool value) {
  prefs_->SetValue(key, std::make_unique<base::Value>(value),
                   WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

int ServiceProcessPrefs::GetInt(const std::string& key,
                                int default_value) const {
  const base::Value* value;
  int result = default_value;
  if (!prefs_->GetValue(key, &value) || !value->GetAsInteger(&result))
    return default_value;

  return result;
}

void ServiceProcessPrefs::SetInt(const std::string& key, int value) {
  prefs_->SetValue(key, std::make_unique<base::Value>(value),
                   WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

const base::DictionaryValue* ServiceProcessPrefs::GetDictionary(
    const std::string& key) const {
  const base::Value* value;
  if (!prefs_->GetValue(key, &value))
    return nullptr;

  const base::DictionaryValue* dict_value = nullptr;
  value->GetAsDictionary(&dict_value);
  return dict_value;
}

const base::ListValue* ServiceProcessPrefs::GetList(
    const std::string& key) const {
  const base::Value* value;
  if (!prefs_->GetValue(key, &value))
    return nullptr;

  const base::ListValue* list_value = nullptr;
  value->GetAsList(&list_value);
  return list_value;
}

void ServiceProcessPrefs::SetValue(const std::string& key,
                                   std::unique_ptr<base::Value> value) {
  prefs_->SetValue(key, std::move(value),
                   WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void ServiceProcessPrefs::RemovePref(const std::string& key) {
  prefs_->RemoveValue(key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}
