// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/physical_web/physical_web_prefs_registration.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "ios/chrome/browser/physical_web/physical_web_constants.h"
#include "ios/chrome/browser/pref_names.h"

void RegisterPhysicalWebBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kIosPhysicalWebEnabled, physical_web::kPhysicalWebOnboarding,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void RegisterPhysicalWebLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kIosPhysicalWebEnabled,
                                physical_web::kPhysicalWebOnboarding);
}
