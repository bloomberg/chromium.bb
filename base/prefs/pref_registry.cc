// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_registry.h"

#include "base/logging.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/pref_store.h"
#include "base/values.h"

PrefRegistry::PrefRegistry()
    : defaults_(new DefaultPrefStore()) {
}

PrefRegistry::~PrefRegistry() {
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

void PrefRegistry::SetRegistrationCallback(
    const RegistrationCallback& callback) {
  registration_callback_ = callback;
}

void PrefRegistry::RegisterPreference(const char* path,
                                      base::Value* default_value) {
  base::Value::Type orig_type = default_value->GetType();
  DCHECK(orig_type != base::Value::TYPE_NULL &&
         orig_type != base::Value::TYPE_BINARY) <<
         "invalid preference type: " << orig_type;
  DCHECK(!defaults_->GetValue(path, NULL)) <<
      "Trying to register a previously registered pref: " << path;

  defaults_->SetDefaultValue(path, default_value);

  if (!registration_callback_.is_null())
    registration_callback_.Run(path, default_value);
}
