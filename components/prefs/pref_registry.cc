// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_registry.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_store.h"

PrefRegistry::PrefRegistry()
    : defaults_(new DefaultPrefStore()) {
}

PrefRegistry::~PrefRegistry() {
}

uint32_t PrefRegistry::GetRegistrationFlags(
    const std::string& pref_name) const {
  const auto& it = registration_flags_.find(pref_name);
  if (it == registration_flags_.end())
    return NO_REGISTRATION_FLAGS;
  return it->second;
}

scoped_refptr<PrefStore> PrefRegistry::defaults() {
  return defaults_.get();
}

PrefRegistry::const_iterator PrefRegistry::begin() const {
  return defaults_->begin();
}

PrefRegistry::const_iterator PrefRegistry::end() const {
  return defaults_->end();
}

void PrefRegistry::SetDefaultPrefValue(const std::string& pref_name,
                                       base::Value* value) {
  DCHECK(value);
  const base::Value* current_value = NULL;
  DCHECK(defaults_->GetValue(pref_name, &current_value))
      << "Setting default for unregistered pref: " << pref_name;
  DCHECK(value->IsType(current_value->type()))
      << "Wrong type for new default: " << pref_name;

  defaults_->ReplaceDefaultValue(pref_name, base::WrapUnique(value));
}

void PrefRegistry::SetDefaultForeignPrefValue(
    const std::string& path,
    std::unique_ptr<base::Value> default_value,
    uint32_t flags) {
  auto erased = foreign_pref_keys_.erase(path);
  DCHECK_EQ(1u, erased);
  RegisterPreference(path, std::move(default_value), flags);
}

void PrefRegistry::RegisterPreference(
    const std::string& path,
    std::unique_ptr<base::Value> default_value,
    uint32_t flags) {
  base::Value::Type orig_type = default_value->type();
  DCHECK(orig_type != base::Value::Type::NONE &&
         orig_type != base::Value::Type::BINARY) <<
         "invalid preference type: " << orig_type;
  DCHECK(!defaults_->GetValue(path, NULL)) <<
      "Trying to register a previously registered pref: " << path;
  DCHECK(!base::ContainsKey(registration_flags_, path))
      << "Trying to register a previously registered pref: " << path;

  defaults_->SetDefaultValue(path, std::move(default_value));
  if (flags != NO_REGISTRATION_FLAGS)
    registration_flags_[path] = flags;
}

void PrefRegistry::RegisterForeignPref(const std::string& path) {
  bool inserted = foreign_pref_keys_.insert(path).second;
  DCHECK(inserted);
}
