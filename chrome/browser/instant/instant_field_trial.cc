// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_field_trial.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace {
bool field_trial_active_ = false;
}

// static
void InstantFieldTrial::Activate() {
  field_trial_active_ = true;
}

// static
InstantFieldTrial::Group InstantFieldTrial::GetGroup(Profile* profile) {
  if (!field_trial_active_)
    return INACTIVE;

  if (profile->IsOffTheRecord())
    return INACTIVE;

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs ||
      prefs->GetBoolean(prefs::kInstantEnabledOnce) ||
      prefs->IsManagedPreference(prefs::kInstantEnabled)) {
    return INACTIVE;
  }

  const int random = prefs->GetInteger(prefs::kInstantFieldTrialRandomDraw);
  return random < 4500 ? CONTROL1 :    // 45%
         random < 9000 ? CONTROL2 :    // 45%
         random < 9500 ? EXPERIMENT1   //  5%
                       : EXPERIMENT2;  //  5%
}

// static
bool InstantFieldTrial::IsExperimentGroup(Profile* profile) {
  Group group = GetGroup(profile);
  return group == EXPERIMENT1 || group == EXPERIMENT2;
}

// static
std::string InstantFieldTrial::GetGroupName(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE:    return "InstantInactive";
    case CONTROL1:    return "InstantControl1";
    case CONTROL2:    return "InstantControl2";
    case EXPERIMENT1: return "InstantExperiment1";
    case EXPERIMENT2: return "InstantExperiment2";
  }
  NOTREACHED();
  return "InstantUnknown";
}
