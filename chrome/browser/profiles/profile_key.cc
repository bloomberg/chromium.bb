// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_key.h"

#include "base/logging.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

ProfileKey::ProfileKey(const base::FilePath& path, ProfileKey* original_key)
    : SimpleFactoryKey(path), prefs_(nullptr), original_key_(original_key) {
  SimpleDependencyManager::GetInstance()->MarkContextLive(this);
}

ProfileKey::~ProfileKey() = default;

PrefService* ProfileKey::GetPrefs() {
  DCHECK(prefs_);
  return prefs_;
}

void ProfileKey::SetPrefs(PrefService* prefs) {
  DCHECK(!prefs_);
  prefs_ = prefs;
}

bool ProfileKey::IsOffTheRecord() const {
  return original_key_ != nullptr;
}

// static
ProfileKey* ProfileKey::FromSimpleFactoryKey(SimpleFactoryKey* key) {
  return key ? static_cast<ProfileKey*>(key) : nullptr;
}
