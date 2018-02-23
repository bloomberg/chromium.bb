// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_logger.h"

#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace policy {

AppInstallEventLogger::AppInstallEventLogger(Delegate* delegate,
                                             Profile* profile) {}

// static
void AppInstallEventLogger::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsRequested);
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsPending);
}

// static
void AppInstallEventLogger::Clear(Profile* profile) {
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsRequested);
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsPending);
}

}  // namespace policy
