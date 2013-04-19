// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/startup_utils.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/file_util.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

// A string pref with initial locale set in VPD or manifest.
const char kInitialLocale[] = "intl.initial_locale";

// A boolean pref of the OOBE complete flag (first OOBE part before login).
const char kOobeComplete[] = "OobeComplete";

// A boolean pref of the device registered flag (second part after first login).
const char kDeviceRegistered[] = "DeviceRegistered";

// Time in seconds that we wait for the device to reboot.
// If reboot didn't happen, ask user to reboot device manually.
const int kWaitForRebootTimeSec = 3;

// Saves boolean "Local State" preference and forces its persistence to disk.
void SaveBoolPreferenceForced(const char* pref_name, bool value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(pref_name, value);
  prefs->CommitPendingWrite();
}

// Saves integer "Local State" preference and forces its persistence to disk.
void SaveIntegerPreferenceForced(const char* pref_name, int value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(pref_name, value);
  prefs->CommitPendingWrite();
}

// Saves string "Local State" preference and forces its persistence to disk.
void SaveStringPreferenceForced(const char* pref_name,
                                const std::string& value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(pref_name, value);
  prefs->CommitPendingWrite();
}

}  // namespace

namespace chromeos {

// static
void StartupUtils::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kOobeComplete, false);
  registry->RegisterIntegerPref(kDeviceRegistered, -1);
  registry->RegisterStringPref(kInitialLocale, "en-US");
}

// static
bool StartupUtils::IsEulaAccepted() {
  return g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

// static
bool StartupUtils::IsOobeCompleted() {
  return g_browser_process->local_state()->GetBoolean(kOobeComplete);
}

// static
void StartupUtils::MarkEulaAccepted() {
  SaveBoolPreferenceForced(prefs::kEulaAccepted, true);
}

// static
void StartupUtils::MarkOobeCompleted() {
  SaveBoolPreferenceForced(kOobeComplete, true);
}

// Returns the path to flag file indicating that both parts of OOBE were
// completed.
// On chrome device, returns /home/chronos/.oobe_completed.
// On Linux desktop, returns $HOME/.oobe_completed.
static base::FilePath GetOobeCompleteFlagPath() {
  // The constant is defined here so it won't be referenced directly.
  const char kOobeCompleteFlagFilePath[] = "/home/chronos/.oobe_completed";

  if (base::chromeos::IsRunningOnChromeOS()) {
    return base::FilePath(kOobeCompleteFlagFilePath);
  } else {
    const char* home = getenv("HOME");
    // Unlikely but if HOME is not defined, use the current directory.
    if (!home)
      home = "";
    return base::FilePath(home).AppendASCII(".oobe_completed");
  }
}

static void CreateOobeCompleteFlagFile() {
  // Create flag file for boot-time init scripts.
  base::FilePath oobe_complete_path = GetOobeCompleteFlagPath();
  if (!file_util::PathExists(oobe_complete_path)) {
    FILE* oobe_flag_file = file_util::OpenFile(oobe_complete_path, "w+b");
    if (oobe_flag_file == NULL)
      DLOG(WARNING) << oobe_complete_path.value() << " doesn't exist.";
    else
      file_util::CloseFile(oobe_flag_file);
  }
}

// static
bool StartupUtils::IsDeviceRegistered() {
  int value = g_browser_process->local_state()->GetInteger(kDeviceRegistered);
  if (value > 0) {
    // Recreate flag file in case it was lost.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CreateOobeCompleteFlagFile));
    return true;
  } else if (value == 0) {
    return false;
  } else {
    // Pref is not set. For compatibility check flag file. It causes blocking
    // IO on UI thread. But it's required for update from old versions.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath oobe_complete_flag_file_path = GetOobeCompleteFlagPath();
    bool file_exists = file_util::PathExists(oobe_complete_flag_file_path);
    SaveIntegerPreferenceForced(kDeviceRegistered, file_exists ? 1 : 0);
    return file_exists;
  }
}

// static
void StartupUtils::MarkDeviceRegistered() {
  SaveIntegerPreferenceForced(kDeviceRegistered, 1);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateOobeCompleteFlagFile));
}

// static
std::string StartupUtils::GetInitialLocale() {
  std::string locale =
      g_browser_process->local_state()->GetString(kInitialLocale);
  if (!l10n_util::IsValidLocaleSyntax(locale))
    locale = "en-US";
  return locale;
}

// static
void StartupUtils::SetInitialLocale(const std::string& locale) {
  if (l10n_util::IsValidLocaleSyntax(locale))
    SaveStringPreferenceForced(kInitialLocale, locale);
  else
    NOTREACHED();
}

}  // namespace chromeos
