// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_util.h"

#include <string>

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace assistant {

ash::mojom::AssistantAllowedState IsAssistantAllowedForProfile(
    const Profile* profile) {
  if (!chromeos::switches::IsAssistantEnabled()) {
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_FLAG;
  }

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER;

  if (profile->IsOffTheRecord())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_INCOGNITO;

  if (profile->IsLegacySupervised())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER;

  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE;

  if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION;

  const std::string kAllowedLocales[] = {ULOC_US, ULOC_UK, ULOC_CANADA,
                                         ULOC_CANADA_FRENCH};

  const PrefService* prefs = profile->GetPrefs();
  std::string pref_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  // Also accept runtime locale which maybe an approximation of user's pref
  // locale.
  const std::string kRuntimeLocale = icu::Locale::getDefault().getName();
  if (!pref_locale.empty()) {
    base::ReplaceChars(pref_locale, "-", "_", &pref_locale);
    bool disallowed = !base::ContainsValue(kAllowedLocales, pref_locale) &&
                      !base::ContainsValue(kAllowedLocales, kRuntimeLocale);

    if (disallowed)
      return ash::mojom::AssistantAllowedState::DISALLOWED_BY_LOCALE;
  }

  return ash::mojom::AssistantAllowedState::ALLOWED;
}

}  // namespace assistant
