// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system/system_api.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#else
#include "chrome/browser/upgrade_detector.h"
#endif

namespace {

// Maps prefs::kIncognitoModeAvailability values (0 = enabled, ...)
// to strings exposed to extensions.
const char* kIncognitoModeAvailabilityStrings[] = {
  "enabled",
  "disabled",
  "forced"
};

// Property keys.
const char kBrightnessKey[] = "brightness";
const char kDownloadProgressKey[] = "downloadProgress";
const char kIsVolumeMutedKey[] = "isVolumeMuted";
const char kStateKey[] = "state";
const char kUserInitiatedKey[] = "userInitiated";
const char kVolumeKey[] = "volume";

// System update states.
const char kNotAvailableState[] = "NotAvailable";
const char kUpdatingState[] = "Updating";
const char kNeedRestartState[] = "NeedRestart";

// Event names.
const char kOnBrightnessChanged[] = "systemPrivate.onBrightnessChanged";
const char kOnVolumeChanged[] = "systemPrivate.onVolumeChanged";
const char kOnScreenUnlocked[] = "systemPrivate.onScreenUnlocked";
const char kOnWokeUp[] = "systemPrivate.onWokeUp";

// Dispatches an extension event with |args|
void DispatchEvent(const std::string& event_name, const ListValue& args) {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (!profile)
    return;
  ExtensionEventRouter* extension_event_router =
      profile->GetExtensionEventRouter();
  if (!extension_event_router)
    return;
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  extension_event_router->DispatchEventToRenderers(
      event_name, json_args, NULL, GURL(), extensions::EventFilteringInfo());
}

}  // namespace

namespace extensions {

bool GetIncognitoModeAvailabilityFunction::RunImpl() {
  PrefService* prefs = profile_->GetPrefs();
  int value = prefs->GetInteger(prefs::kIncognitoModeAvailability);
  EXTENSION_FUNCTION_VALIDATE(
      value >= 0 &&
      value < static_cast<int>(arraysize(kIncognitoModeAvailabilityStrings)));
  result_.reset(
      Value::CreateStringValue(kIncognitoModeAvailabilityStrings[value]));
  return true;
}

bool GetUpdateStatusFunction::RunImpl() {
  std::string state;
  double download_progress = 0;
#if defined(OS_CHROMEOS)
  // With UpdateEngineClient, we can provide more detailed information about
  // system updates on ChromeOS.
  const chromeos::UpdateEngineClient::Status status =
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
      GetLastStatus();
  // |download_progress| is set to 1 after download finishes
  // (i.e. verify, finalize and need-reboot phase) to indicate the progress
  // even though |status.download_progress| is 0 in these phases.
  switch (status.status) {
    case chromeos::UpdateEngineClient::UPDATE_STATUS_ERROR:
      state = kNotAvailableState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_IDLE:
      state = kNotAvailableState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      state = kNotAvailableState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      state = kUpdatingState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      state = kUpdatingState;
      download_progress = status.download_progress;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_VERIFYING:
      state = kUpdatingState;
      download_progress = 1;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      state = kUpdatingState;
      download_progress = 1;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      state = kNeedRestartState;
      download_progress = 1;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      state = kNotAvailableState;
      break;
  }
#else
  if (UpgradeDetector::GetInstance()->notify_upgrade()) {
    state = kNeedRestartState;
    download_progress = 1;
  } else {
    state = kNotAvailableState;
  }
#endif
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kStateKey, state);
  dict->SetDouble(kDownloadProgressKey, download_progress);
  result_.reset(dict);

  return true;
}

void DispatchVolumeChangedEvent(double volume, bool is_volume_muted) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetDouble(kVolumeKey, volume);
  dict->SetBoolean(kIsVolumeMutedKey, is_volume_muted);
  args.Append(dict);
  DispatchEvent(kOnVolumeChanged, args);
}

void DispatchBrightnessChangedEvent(int brightness, bool user_initiated) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger(kBrightnessKey, brightness);
  dict->SetBoolean(kUserInitiatedKey, user_initiated);
  args.Append(dict);
  DispatchEvent(kOnBrightnessChanged, args);
}

void DispatchScreenUnlockedEvent() {
  ListValue args;
  DispatchEvent(kOnScreenUnlocked, args);
}

void DispatchWokeUpEvent() {
  ListValue args;
  DispatchEvent(kOnWokeUp, args);
}

}  // namespace extensions
