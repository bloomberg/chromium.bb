// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif  // OS_WIN

// static
bool IncognitoModePrefs::IntToAvailability(int in_value,
                                           Availability* out_value) {
  if (in_value < 0 || in_value >= AVAILABILITY_NUM_TYPES) {
    *out_value = ENABLED;
    return false;
  }
  *out_value = static_cast<Availability>(in_value);
  return true;
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailability(
    const PrefService* pref_service) {
  DCHECK(pref_service);
  int pref_value = pref_service->GetInteger(prefs::kIncognitoModeAvailability);
  Availability result = IncognitoModePrefs::ENABLED;
  bool valid = IntToAvailability(pref_value, &result);
  DCHECK(valid);
#if defined(OS_WIN)
  // Disable incognito mode windows if parental controls are on. This is only
  // for Windows Vista and above.
  if (base::win::IsParentalControlActivityLoggingOn()) {
    if (result == IncognitoModePrefs::FORCED)
      LOG(ERROR) << "Ignoring FORCED incognito. Parental control logging on";
    return IncognitoModePrefs::DISABLED;
  }
#endif  // OS_WIN
  return result;
}

// static
void IncognitoModePrefs::SetAvailability(PrefService* prefs,
                                         const Availability availability) {
  prefs->SetInteger(prefs::kIncognitoModeAvailability, availability);
}

// static
void IncognitoModePrefs::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kIncognitoModeAvailability,
                                IncognitoModePrefs::ENABLED,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognito(
    const CommandLine& command_line,
    const PrefService* prefs) {
  Availability incognito_avail = GetAvailability(prefs);
  return incognito_avail != IncognitoModePrefs::DISABLED &&
         (command_line.HasSwitch(switches::kIncognito) ||
          incognito_avail == IncognitoModePrefs::FORCED);
}
