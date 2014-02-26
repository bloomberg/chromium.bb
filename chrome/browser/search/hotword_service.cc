// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const int kMaxTimesToShowOptInPopup = 10;

// Enum describing the state of the hotword preference.
// This is used for UMA stats -- do not reorder or delete items; only add to
// the end.
enum HotwordEnabled {
  UNSET = 0,  // The hotword preference has not been set.
  ENABLED,    // The hotword preference is enabled.
  DISABLED,   // The hotword preference is disabled.
  NUM_HOTWORD_ENABLED_METRICS
};

// Enum describing the availability state of the hotword extension.
// This is used for UMA stats -- do not reorder or delete items; only add to
// the end.
enum HotwordExtensionAvailability {
  UNAVAILABLE = 0,
  AVAILABLE,
  PENDING_DOWNLOAD,
  DISABLED_EXTENSION,
  NUM_HOTWORD_EXTENSION_AVAILABILITY_METRICS
};

void RecordAvailabilityMetrics(
    ExtensionService* service,
    const extensions::Extension* extension) {
  HotwordExtensionAvailability availability_state = UNAVAILABLE;
  if (extension) {
    availability_state = AVAILABLE;
  } else if (service->pending_extension_manager() &&
             service->pending_extension_manager()->IsIdPending(
                 extension_misc::kHotwordExtensionId)) {
    availability_state = PENDING_DOWNLOAD;
  } else if (!service->IsExtensionEnabled(
      extension_misc::kHotwordExtensionId)) {
    availability_state = DISABLED_EXTENSION;
  }
  UMA_HISTOGRAM_ENUMERATION("Hotword.HotwordExtensionAvailability",
                            availability_state,
                            NUM_HOTWORD_EXTENSION_AVAILABILITY_METRICS);
}

}  // namespace

namespace hotword_internal {
// Constants for the hotword field trial.
const char kHotwordFieldTrialName[] = "VoiceTrigger";
const char kHotwordFieldTrialDisabledGroupName[] = "Disabled";
}  // namespace hotword_internal

// static
bool HotwordService::DoesHotwordSupportLanguage(Profile* profile) {
  std::string locale =
#if defined(OS_CHROMEOS)
      // On ChromeOS locale is per-profile.
      profile->GetPrefs()->GetString(prefs::kApplicationLocale);
#else
      g_browser_process->GetApplicationLocale();
#endif
  // Only available for English now.
  std::string normalized_locale = l10n_util::NormalizeLocale(locale);
  return normalized_locale == "en" || normalized_locale == "en_us" ||
      normalized_locale =="en_US";
}

HotwordService::HotwordService(Profile* profile)
    : profile_(profile) {
  // This will be called during profile initialization which is a good time
  // to check the user's hotword state.
  HotwordEnabled enabled_state = UNSET;
  if (profile_->GetPrefs()->HasPrefPath(prefs::kHotwordSearchEnabled)) {
    if (profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled))
      enabled_state = ENABLED;
    else
      enabled_state = DISABLED;
  }
  UMA_HISTOGRAM_ENUMERATION("Hotword.Enabled", enabled_state,
                            NUM_HOTWORD_ENABLED_METRICS);
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

  RecordAvailabilityMetrics(service, extension);

  return extension && IsHotwordAllowed();
}

bool HotwordService::IsHotwordAllowed() {
  std::string group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  return !group.empty() &&
      group != hotword_internal::kHotwordFieldTrialDisabledGroupName &&
      DoesHotwordSupportLanguage(profile_);
}

bool HotwordService::RetryHotwordExtension() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  if (!extension_system || !extension_system->extension_service())
    return false;
  ExtensionService* extension_service = extension_system->extension_service();

  extension_service->ReloadExtension(extension_misc::kHotwordExtensionId);
  return true;
}
