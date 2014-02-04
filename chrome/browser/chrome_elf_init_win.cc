// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/chrome_elf_init_win.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "version.h"  // NOLINT

namespace {

const char kBrowserBlacklistTrialName[] = "BrowserBlacklist";
const char kBrowserBlacklistTrialEnabledGroupName[] = "Enabled";

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
}

void BrowserBlacklistBeaconSetup() {
  base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER,
                                           blacklist::kRegistryBeaconPath,
                                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  // Find the last recorded blacklist version.
  base::string16 blacklist_version;
  blacklist_registry_key.ReadValue(blacklist::kBeaconVersion,
                                   &blacklist_version);

  if (blacklist_version != TEXT(CHROME_VERSION_STRING)) {
    // The blacklist hasn't run for this version yet, so enable it.
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

    // Don't try to record if the blacklist setup succeeded or failed in the
    // run since it could have been from either this version or the previous
    // version (since crashes occur before we set the version in the registry).
  } else {
    // The blacklist version didn't change, so record the results of the
    // latest setup.
    DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
    blacklist_registry_key.ReadValueDW(blacklist::kBeaconState,
                                       &blacklist_state);

    // Record the results of the latest blacklist setup.
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

      // Since some part of the blacklist failed, ensure it is now disabled
      // for this version.
      if (blacklist_state != blacklist::BLACKLIST_DISABLED) {
        blacklist_registry_key.WriteValue(blacklist::kBeaconState,
                                          blacklist::BLACKLIST_DISABLED);
      }
    }
  }
}
