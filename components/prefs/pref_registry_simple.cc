// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_registry_simple.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

PrefRegistrySimple::PrefRegistrySimple() {
}

PrefRegistrySimple::~PrefRegistrySimple() {
}

void PrefRegistrySimple::RegisterBooleanPref(const std::string& path,
                                             bool default_value) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterIntegerPref(const std::string& path,
                                             int default_value) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterDoublePref(const std::string& path,
                                            double default_value) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterStringPref(const std::string& path,
                                            const std::string& default_value) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterFilePathPref(
    const std::string& path,
    const base::FilePath& default_value) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value.value()),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterListPref(const std::string& path) {
  RegisterPreference(path, base::MakeUnique<base::ListValue>(),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterListPref(
    const std::string& path,
    std::unique_ptr<base::ListValue> default_value) {
  RegisterPreference(path, std::move(default_value), NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterDictionaryPref(const std::string& path) {
  RegisterPreference(path, base::MakeUnique<base::DictionaryValue>(),
                     NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterDictionaryPref(
    const std::string& path,
    std::unique_ptr<base::DictionaryValue> default_value) {
  RegisterPreference(path, std::move(default_value), NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterInt64Pref(const std::string& path,
                                           int64_t default_value) {
  RegisterPreference(
      path, base::MakeUnique<base::Value>(base::Int64ToString(default_value)),
      NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterUint64Pref(const std::string& path,
                                            uint64_t default_value) {
  RegisterPreference(
      path, base::MakeUnique<base::Value>(base::Uint64ToString(default_value)),
      NO_REGISTRATION_FLAGS);
}

void PrefRegistrySimple::RegisterBooleanPref(const std::string& path,
                                             bool default_value,
                                             uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value), flags);
}

void PrefRegistrySimple::RegisterIntegerPref(const std::string& path,
                                             int default_value,
                                             uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value), flags);
}

void PrefRegistrySimple::RegisterDoublePref(const std::string& path,
                                            double default_value,
                                            uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value), flags);
}

void PrefRegistrySimple::RegisterStringPref(const std::string& path,
                                            const std::string& default_value,
                                            uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value), flags);
}

void PrefRegistrySimple::RegisterFilePathPref(
    const std::string& path,
    const base::FilePath& default_value,
    uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::Value>(default_value.value()),
                     flags);
}

void PrefRegistrySimple::RegisterListPref(const std::string& path,
                                          uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::ListValue>(), flags);
}

void PrefRegistrySimple::RegisterListPref(
    const std::string& path,
    std::unique_ptr<base::ListValue> default_value,
    uint32_t flags) {
  RegisterPreference(path, std::move(default_value), flags);
}

void PrefRegistrySimple::RegisterDictionaryPref(const std::string& path,
                                                uint32_t flags) {
  RegisterPreference(path, base::MakeUnique<base::DictionaryValue>(), flags);
}

void PrefRegistrySimple::RegisterDictionaryPref(
    const std::string& path,
    std::unique_ptr<base::DictionaryValue> default_value,
    uint32_t flags) {
  RegisterPreference(path, std::move(default_value), flags);
}

void PrefRegistrySimple::RegisterInt64Pref(const std::string& path,
                                           int64_t default_value,
                                           uint32_t flags) {
  RegisterPreference(
      path, base::MakeUnique<base::Value>(base::Int64ToString(default_value)),
      flags);
}

void PrefRegistrySimple::RegisterUint64Pref(const std::string& path,
                                            uint64_t default_value,
                                            uint32_t flags) {
  RegisterPreference(
      path, base::MakeUnique<base::Value>(base::Uint64ToString(default_value)),
      flags);
}
