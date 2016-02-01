// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_gcm_manager.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/proximity_auth/cryptauth/pref_names.h"

namespace proximity_auth {

CryptAuthGCMManager::Observer::~Observer() {
}

void CryptAuthGCMManager::Observer::OnGCMRegistrationResult(bool success) {
}

void CryptAuthGCMManager::Observer::OnReenrollMessage() {
}

void CryptAuthGCMManager::Observer::OnResyncMessage() {
}

// static.
void CryptAuthGCMManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthGCMRegistrationId,
                               std::string());
}

}  // namespace proximity_auth
