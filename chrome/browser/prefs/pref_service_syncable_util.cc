// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_util.h"

#include <vector>

#include "chrome/browser/prefs/pref_service_incognito_whitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_preferences/pref_service_syncable.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#endif  // defined(OS_CHROMEOS)

sync_preferences::PrefServiceSyncable* PrefServiceSyncableFromProfile(
    Profile* profile) {
  return static_cast<sync_preferences::PrefServiceSyncable*>(
      profile->GetPrefs());
}

sync_preferences::PrefServiceSyncable* PrefServiceSyncableIncognitoFromProfile(
    Profile* profile) {
  return static_cast<sync_preferences::PrefServiceSyncable*>(
      profile->GetOffTheRecordPrefs());
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* pref_service,
    PrefStore* incognito_extension_pref_store,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  // List of keys that can be changed in the user prefs file by the incognito
  // profile.
  static const char* persistent_pref_names[] = {
#if defined(OS_CHROMEOS)
    // Accessibility preferences should be persisted if they are changed in
    // incognito mode.
    ash::prefs::kAccessibilityLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorDipSize,
    ash::prefs::kAccessibilityStickyKeysEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
    ash::prefs::kAccessibilityScreenMagnifierEnabled,
    ash::prefs::kAccessibilityScreenMagnifierScale,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityMonoAudioEnabled,
    ash::prefs::kAccessibilityAutoclickEnabled,
    ash::prefs::kAccessibilityAutoclickDelayMs,
    ash::prefs::kAccessibilityCaretHighlightEnabled,
    ash::prefs::kAccessibilityCursorHighlightEnabled,
    ash::prefs::kAccessibilityFocusHighlightEnabled,
    ash::prefs::kAccessibilitySelectToSpeakEnabled,
    ash::prefs::kAccessibilitySwitchAccessEnabled,
    ash::prefs::kAccessibilityDictationEnabled,
    ash::prefs::kDockedMagnifierEnabled,
    ash::prefs::kDockedMagnifierScale,
    ash::prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    ash::prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu
#endif  // defined(OS_CHROMEOS)
  };

  // TODO(https://crbug.com/861722): Remove |GetIncognitoWhitelist|, its
  // file, and |persistent_prefs|.
  // This list is ONLY added for transition of code from blacklist to whitelist.
  std::vector<const char*> persistent_prefs;
  prefs::GetIncognitoWhitelist(&persistent_prefs);
  persistent_prefs.insert(
      persistent_prefs.end(), persistent_pref_names,
      persistent_pref_names + sizeof(persistent_pref_names) / sizeof(char*));

  // TODO(https://crbug.com/861722): Current implementation does not cover
  // preferences from iOS. The code should be refactored to cover it.

  return pref_service->CreateIncognitoPrefService(
      incognito_extension_pref_store, persistent_prefs, std::move(delegate));
}
