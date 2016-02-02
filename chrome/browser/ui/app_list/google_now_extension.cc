// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/google_now_extension.h"

#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ui/message_center/notifier_settings.h"

namespace {
const char kGoogleNowExtensionFieldTrialName[] = "GoogleNowExtension";

// Extension ID for previous Now notifications component.
const char kNowNotifierId[] = "pafkbggdmjlpgkdkcbjmhmfcdpncadgh";
};  // namespace

bool GetGoogleNowExtensionId(std::string* extension_id) {
  *extension_id = variations::GetVariationParamValue(
      kGoogleNowExtensionFieldTrialName, "id");
  return !extension_id->empty();
}

// TODO(skare): Remove this if/when the Now Notifications component is
// deprecated. http://crbug.com/459846
void MigrateGoogleNowPrefs(Profile* profile) {
  std::string now_extension_id;
  if (!GetGoogleNowExtensionId(&now_extension_id))
    return;

  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* enabled_pref =
      prefs->FindPreference(prefs::kGoogleNowLauncherEnabled);

  // If the pref is not its default value, migration was performed.
  if (!enabled_pref->IsDefaultValue())
    return;

  NotifierStateTracker* const notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(profile);
  bool notifier_enabled = notifier_state_tracker->IsNotifierEnabled(
      message_center::NotifierId(
          message_center::NotifierId::APPLICATION, kNowNotifierId));
  prefs->SetBoolean(prefs::kGoogleNowLauncherEnabled, notifier_enabled);
}
