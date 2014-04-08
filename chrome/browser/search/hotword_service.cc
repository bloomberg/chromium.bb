// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const int kMaxTimesToShowOptInPopup = 10;

// Allowed languages for hotwording.
static const char* kSupportedLocales[] = {
  "en",
  "en_us",
  "en_gb",
  "en_ca",
  "en_au",
  "fr_fr",
  "de_de",
  "ru_ru"
};

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

void RecordLoggingMetrics(Profile* profile) {
  // If the user is not opted in to hotword voice search, the audio logging
  // metric is not valid so it is not recorded.
  if (!profile->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled))
    return;

  UMA_HISTOGRAM_BOOLEAN(
      "Hotword.HotwordAudioLogging",
      profile->GetPrefs()->GetBoolean(prefs::kHotwordAudioLoggingEnabled));
}

ExtensionService* GetExtensionService(Profile* profile) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  if (extension_system)
    return extension_system->extension_service();
  return NULL;
}

}  // namespace

namespace hotword_internal {
// Constants for the hotword field trial.
const char kHotwordFieldTrialName[] = "VoiceTrigger";
const char kHotwordFieldTrialDisabledGroupName[] = "Disabled";
// Old preference constant.
const char kHotwordUnusablePrefName[] = "hotword.search_enabled";
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
  std::string normalized_locale = l10n_util::NormalizeLocale(locale);
  StringToLowerASCII(&normalized_locale);

  for (size_t i = 0; i < arraysize(kSupportedLocales); i++) {
    if (kSupportedLocales[i] == normalized_locale)
      return true;
  }
  return false;
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
  } else {
    // If the preference has not been set the hotword extension should
    // not be running.
    DisableHotwordExtension(GetExtensionService(profile_));
  }
  UMA_HISTOGRAM_ENUMERATION("Hotword.Enabled", enabled_state,
                            NUM_HOTWORD_ENABLED_METRICS);

  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kHotwordSearchEnabled,
      base::Bind(&HotwordService::OnHotwordSearchEnabledChanged,
                 base::Unretained(this)));

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));

  // Clear the old user pref because it became unusable.
  // TODO(rlp): Remove this code per crbug.com/358789.
  if (profile_->GetPrefs()->HasPrefPath(
          hotword_internal::kHotwordUnusablePrefName)) {
    profile_->GetPrefs()->ClearPref(hotword_internal::kHotwordUnusablePrefName);
  }
}

HotwordService::~HotwordService() {
}

void HotwordService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_INSTALLED) {
    const extensions::Extension* extension =
        content::Details<const extensions::InstalledExtensionInfo>(details)
              ->extension;
    if (extension->id() == extension_misc::kHotwordExtensionId &&
        !profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled)) {
      DisableHotwordExtension(GetExtensionService(profile_));
      // Once the extension is disabled, it will not be enabled until the
      // user opts in at which point the pref registrar will take over
      // enabling and disabling.
      registrar_.Remove(this,
                        chrome::NOTIFICATION_EXTENSION_INSTALLED,
                        content::Source<Profile>(profile_));
    }
  }
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
  // Include disabled extensions (true parameter) since it may not be enabled
  // if the user opted out.
  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kHotwordExtensionId, true);

  RecordAvailabilityMetrics(service, extension);
  RecordLoggingMetrics(profile_);

  return extension && IsHotwordAllowed();
}

bool HotwordService::IsHotwordAllowed() {
  std::string group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  return !group.empty() &&
      group != hotword_internal::kHotwordFieldTrialDisabledGroupName &&
      DoesHotwordSupportLanguage(profile_);
}

bool HotwordService::IsOptedIntoAudioLogging() {
  // Do not opt the user in if the preference has not been set.
  return
      profile_->GetPrefs()->HasPrefPath(prefs::kHotwordAudioLoggingEnabled) &&
      profile_->GetPrefs()->GetBoolean(prefs::kHotwordAudioLoggingEnabled);
}

bool HotwordService::RetryHotwordExtension() {
  ExtensionService* extension_service = GetExtensionService(profile_);
  if (!extension_service)
    return false;

  extension_service->ReloadExtension(extension_misc::kHotwordExtensionId);
  return true;
}

void HotwordService::EnableHotwordExtension(
    ExtensionService* extension_service) {
  if (extension_service)
    extension_service->EnableExtension(extension_misc::kHotwordExtensionId);
}

void HotwordService::DisableHotwordExtension(
    ExtensionService* extension_service) {
  if (extension_service) {
    extension_service->DisableExtension(
        extension_misc::kHotwordExtensionId,
        extensions::Extension::DISABLE_USER_ACTION);
  }
}

void HotwordService::OnHotwordSearchEnabledChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, std::string(prefs::kHotwordSearchEnabled));

  ExtensionService* extension_service = GetExtensionService(profile_);
  if (profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled))
    EnableHotwordExtension(extension_service);
  else
    DisableHotwordExtension(extension_service);
}
