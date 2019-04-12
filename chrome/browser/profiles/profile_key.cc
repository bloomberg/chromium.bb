// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_key.h"

ProfileKey::ProfileKey(const base::FilePath& path,
                       PrefService* prefs,
                       ProfileKey* original_key)
    : SimpleFactoryKey(path), prefs_(prefs), original_key_(original_key) {}

ProfileKey::~ProfileKey() = default;

bool ProfileKey::IsOffTheRecord() const {
  return original_key_ != nullptr;
}

// static
ProfileKey* ProfileKey::FromSimpleFactoryKey(SimpleFactoryKey* key) {
  return key ? static_cast<ProfileKey*>(key) : nullptr;
}
