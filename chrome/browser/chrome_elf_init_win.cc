// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/chrome_elf_init_win.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/dll_hash/dll_hash.h"
#include "content/public/browser/browser_thread.h"
#include "version.h"  // NOLINT

namespace {

const char kBrowserBlacklistTrialName[] = "BrowserBlacklist";
const char kBrowserBlacklistTrialEnabledGroupName[] = "Enabled";

// How long to wait, in seconds, before reporting for the second (and last
// time), what dlls were blocked from the browser process.
const int kBlacklistReportingDelaySec = 600;

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended in front of
//       BLACKLIST_SETUP_EVENT_MAX.
enum BlacklistSetupEventType {
  // The blacklist beacon has placed to enable the browser blacklisting.
  BLACKLIST_SETUP_ENABLED = 0,

  // The blacklist was successfully enabled.
  BLACKLIST_SETUP_RAN_SUCCESSFULLY,

  // The blacklist setup code failed to execute.
  BLACKLIST_SETUP_FAILED,

  // The blacklist thunk setup code failed to execute.
  BLACKLIST_THUNK_SETUP_FAILED,

  // The blacklist interception code failed to execute.
  BLACKLIST_INTERCEPTION_FAILED,

  // Always keep this at the end.
  BLACKLIST_SETUP_EVENT_MAX,
};

void RecordBlacklistSetupEvent(BlacklistSetupEventType blacklist_setup_event) {
  UMA_HISTOGRAM_ENUMERATION("Blacklist.Setup",
                            blacklist_setup_event,
                            BLACKLIST_SETUP_EVENT_MAX);
}

// Report which DLLs were prevented from being loaded.
void ReportSuccessfulBlocks() {
  // Figure out how many dlls were blocked.
  int num_blocked_dlls = 0;
  blacklist::SuccessfullyBlocked(NULL, &num_blocked_dlls);

  if (num_blocked_dlls == 0)
    return;

  // Now retrieve the list of blocked dlls.
  std::vector<const wchar_t*> blocked_dlls(num_blocked_dlls);
  blacklist::SuccessfullyBlocked(&blocked_dlls[0], &num_blocked_dlls);

  // Send up the hashes of the blocked dlls via UMA.
  for (size_t i = 0; i < blocked_dlls.size(); ++i) {
    std::string dll_name_utf8;
    base::WideToUTF8(blocked_dlls[i], wcslen(blocked_dlls[i]), &dll_name_utf8);
    int uma_hash = DllNameToHash(dll_name_utf8);

    UMA_HISTOGRAM_SPARSE_SLOWLY("Blacklist.Blocked", uma_hash);
  }
}

}  // namespace

void InitializeChromeElf() {
  if (base::FieldTrialList::FindFullName(kBrowserBlacklistTrialName) ==
      kBrowserBlacklistTrialEnabledGroupName) {
    BrowserBlacklistBeaconSetup();
  } else {
    // Disable the blacklist for all future runs by removing the beacon.
    base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER);
    blacklist_registry_key.DeleteKey(blacklist::kRegistryBeaconPath);
  }

  // Report all successful blacklist interceptions.
  ReportSuccessfulBlocks();

  // Schedule another task to report all sucessful interceptions later.
  // This time delay should be long enough to catch any dlls that attempt to
  // inject after Chrome has started up.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ReportSuccessfulBlocks),
      base::TimeDelta::FromSeconds(kBlacklistReportingDelaySec));
}

void BrowserBlacklistBeaconSetup() {
  base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER,
                                           blacklist::kRegistryBeaconPath,
                                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  // No point in trying to continue if the registry key isn't valid.
  if (!blacklist_registry_key.Valid())
    return;

  // Record the results of the last blacklist setup.
  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  blacklist_registry_key.ReadValueDW(blacklist::kBeaconState, &blacklist_state);

  if (blacklist_state == blacklist::BLACKLIST_ENABLED) {
    RecordBlacklistSetupEvent(BLACKLIST_SETUP_RAN_SUCCESSFULLY);
  } else {
    switch (blacklist_state) {
      case blacklist::BLACKLIST_SETUP_RUNNING:
        RecordBlacklistSetupEvent(BLACKLIST_SETUP_FAILED);
        break;
      case blacklist::BLACKLIST_THUNK_SETUP:
        RecordBlacklistSetupEvent(BLACKLIST_THUNK_SETUP_FAILED);
        break;
      case blacklist::BLACKLIST_INTERCEPTING:
        RecordBlacklistSetupEvent(BLACKLIST_INTERCEPTION_FAILED);
        break;
    }

    // Since some part of the blacklist failed, mark it as disabled
    // for this version.
    if (blacklist_state != blacklist::BLACKLIST_DISABLED) {
      blacklist_registry_key.WriteValue(blacklist::kBeaconState,
                                        blacklist::BLACKLIST_DISABLED);
    }
  }

  // Find the last recorded blacklist version.
  base::string16 blacklist_version;
  blacklist_registry_key.ReadValue(blacklist::kBeaconVersion,
                                   &blacklist_version);

  if (blacklist_version != TEXT(CHROME_VERSION_STRING)) {
    // The blacklist hasn't been enabled for this version yet, so enable it.
    LONG set_version = blacklist_registry_key.WriteValue(
        blacklist::kBeaconVersion,
        TEXT(CHROME_VERSION_STRING));

    LONG set_state = blacklist_registry_key.WriteValue(
        blacklist::kBeaconState,
        blacklist::BLACKLIST_ENABLED);

    // Only report the blacklist as getting setup when both registry writes
    // succeed, since otherwise the blacklist wasn't properly setup.
    if (set_version == ERROR_SUCCESS && set_state == ERROR_SUCCESS)
      RecordBlacklistSetupEvent(BLACKLIST_SETUP_ENABLED);
  }
}
