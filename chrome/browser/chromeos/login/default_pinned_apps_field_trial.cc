// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"

#include "base/basictypes.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"

namespace chromeos {
namespace default_pinned_apps_field_trial {

namespace {

// Name of a local state pref to store the list of users that participate
// the experiment.
const char kExperimentUsers[] = "DefaultPinnedAppsExperimentUsers";

}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kExperimentUsers);
}

void MigratePrefs(PrefService* local_state) {
  // TODO(xiyuan): Remove everything in M34.
  local_state->ClearPref(kExperimentUsers);
}

}  // namespace default_pinned_apps_field_trial
}  // namespace chromeos
