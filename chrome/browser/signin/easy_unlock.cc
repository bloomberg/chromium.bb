// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"

namespace easy_unlock {

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockShowTutorial,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kEasyUnlockPairing,
      new base::DictionaryValue(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void LaunchEasyUnlockSetup(Profile* profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kEasyUnlockAppId, false);

  OpenApplication(AppLaunchParams(
      profile, extension, extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW));
}

bool IsEnabled() {
  // FindFullName() call needs to happen before command line check to activate
  // the trial in the right group. See go/finch-and-flags.
  const std::string group_name =
      base::FieldTrialList::FindFullName("EasyUnlock");
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableEasyUnlock)) {
    return true;
  }
  return group_name == "Enable";
}

}  // namespace easy_unlock
