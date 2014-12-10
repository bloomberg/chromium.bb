// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"

#include "base/prefs/pref_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace password_manager {
namespace urls_collection_experiment {

namespace {

bool ShouldShowBubbleExperiment(PrefService* prefs) {
  // TODO(melandory): Make decision based on Finch experiment parameters.
  return false;
}

}  // namespace

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      password_manager::prefs::kWasAllowToCollectURLBubbleShown,
      false,  // bubble hasn't been shown yet
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

const char kExperimentName[] = "AskToSubmitURLBubble";

bool ShouldShowBubble(PrefService* prefs) {
  if (prefs->GetBoolean(prefs::kWasAllowToCollectURLBubbleShown)) {
    return ShouldShowBubbleExperiment(prefs);
  }
  // "Do not show" is the default case.
  return false;
}

void RecordBubbleClosed(PrefService* prefs) {
  prefs->SetBoolean(password_manager::prefs::kWasAllowToCollectURLBubbleShown,
                    true);
}

}  // namespace urls_collection_experiment
}  // namespace password_manager
