// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/prefs.h"

#include "apps/pref_names.h"
#include "base/prefs/pref_registry_simple.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace apps {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
#if !defined(OS_MACOSX)
  registry->RegisterBooleanPref(
      prefs::kAppFullscreenAllowed, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

}  // namespace apps
