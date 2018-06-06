// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"

UnifiedConsentService::UnifiedConsentService(
    PrefService* pref_service,
    identity::IdentityManager* identity_manager)
    : pref_service_(pref_service), identity_manager_(identity_manager) {
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  identity_manager_->AddObserver(this);
}

UnifiedConsentService::~UnifiedConsentService() {}

void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
}

void UnifiedConsentService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  // By design, signing out of Chrome automatically disables the user consent
  // for making search and browsing better.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            false);
}
