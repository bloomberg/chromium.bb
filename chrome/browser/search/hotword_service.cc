// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const int kMaxTimesToShowOptInPopup = 10;
}

namespace hotword_internal {
// Constants for the hotword field trial.
const char kHotwordFieldTrialName[] = "VoiceTrigger";
const char kHotwordFieldTrialDisabledGroupName[] = "Disabled";
}  // namespace hotword_internal

HotwordService::HotwordService(Profile* profile)
    : profile_(profile) {
}

HotwordService::~HotwordService() {
}

bool HotwordService::ShouldShowOptInPopup() {
  if (profile_->IsOffTheRecord())
    return false;

  // Profile is not off the record.
  if (profile_->GetPrefs()->HasPrefPath(prefs::kHotwordSearchEnabled))
    return false;  // Already opted in or opted out;

  int number_shown = profile_->GetPrefs()->GetInteger(
      prefs::kHotwordOptInPopupTimesShown);
  return number_shown < MaxNumberTimesToShowOptInPopup();
}

int HotwordService::MaxNumberTimesToShowOptInPopup() {
  return kMaxTimesToShowOptInPopup;
}

void HotwordService::ShowOptInPopup() {
  int number_shown = profile_->GetPrefs()->GetInteger(
      prefs::kHotwordOptInPopupTimesShown);
  profile_->GetPrefs()->SetInteger(prefs::kHotwordOptInPopupTimesShown,
                                   ++number_shown);
  // TODO(rlp): actually show opt in popup when linked up to extension.
}

bool HotwordService::IsServiceAvailable() {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  ExtensionService* service = system->extension_service();
  // Do not include disabled extension (false parameter) because if the
  // extension is disabled, it's not available.
  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kHotwordExtensionId, false);
  return extension && IsHotwordAllowed();
}

bool HotwordService::IsHotwordAllowed() {
  std::string group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  if (!group.empty() &&
      group != hotword_internal::kHotwordFieldTrialDisabledGroupName) {
    std::string locale =
#if defined(OS_CHROMEOS)
        // On ChromeOS locale is per-profile.
        profile_->GetPrefs()->GetString(prefs::kApplicationLocale);
#else
        g_browser_process->GetApplicationLocale();
#endif
    // Only available for English now.
    std::string normalized_locale = l10n_util::NormalizeLocale(locale);
    return normalized_locale == "en" || normalized_locale == "en_us" ||
        normalized_locale =="en_US";
  }
  return false;
}
