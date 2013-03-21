// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Key under "kiosk" dictionary to store last launch error.
const char kKeyLaunchError[] = "launch_error";

}  // namespace

// static
std::string KioskAppLaunchError::GetErrorMessage(Error error) {
  switch (error) {
    case NONE:
      return std::string();

    case HAS_PENDING_LAUNCH:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_FAILED_TO_LAUNCH);

    case CRYPTOHOMED_NOT_RUNNING:
    case ALREADY_MOUNTED:
    case UNABLE_TO_MOUNT:
    case UNABLE_TO_REMOVE:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_MOUNT);

    case UNABLE_TO_INSTALL:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_INSTALL);

    case USER_CANCEL:
      return l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_USER_CANCEL);
  }

  NOTREACHED() << "Unknown kiosk app launch error, error=" << error;
  return l10n_util::GetStringUTF8(IDS_KIOSK_APP_FAILED_TO_LAUNCH);
}

// static
void KioskAppLaunchError::Save(KioskAppLaunchError::Error error) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetInteger(kKeyLaunchError, error);
}

// static
KioskAppLaunchError::Error KioskAppLaunchError::Get() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

  int error;
  if (dict->GetInteger(kKeyLaunchError, &error))
    return static_cast<KioskAppLaunchError::Error>(error);

  return KioskAppLaunchError::NONE;
}

// static
void KioskAppLaunchError::Clear() {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->Remove(kKeyLaunchError, NULL);
}

}  // namespace chromeos
