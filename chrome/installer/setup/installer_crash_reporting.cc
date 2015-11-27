// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_crash_reporting.h"

#include "base/debug/crash_logging.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/setup/installer_crash_reporter_client.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/installer_state.h"
#include "components/crash/content/app/breakpad_win.h"
#include "components/crash/content/app/crash_keys_win.h"
#include "components/crash/core/common/crash_keys.h"

namespace installer {

namespace {

// Crash Keys

const char kDistributionType[] = "dist-type";
const char kIsMultiInstall[] = "multi-install";
const char kIsSystemLevel[] = "system-level";
const char kOperation[] = "operation";
const char kStateKey[] = "state-key";

#if defined(COMPONENT_BUILD)
// Installed via base::debug::SetCrashKeyReportingFunctions.
void SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value) {
  DCHECK(breakpad::CrashKeysWin::keeper());
  breakpad::CrashKeysWin::keeper()->SetCrashKeyValue(base::UTF8ToUTF16(key),
                                                     base::UTF8ToUTF16(value));
}

// Installed via base::debug::SetCrashKeyReportingFunctions.
void ClearCrashKey(const base::StringPiece& key) {
  DCHECK(breakpad::CrashKeysWin::keeper());
  breakpad::CrashKeysWin::keeper()->ClearCrashKeyValue(base::UTF8ToUTF16(key));
}
#endif  // COMPONENT_BUILD

const char *DistributionTypeToString(BrowserDistribution::Type type) {
  switch (type) {
    case BrowserDistribution::CHROME_BROWSER:
      return "chrome browser";
    case BrowserDistribution::CHROME_FRAME:
      return "chrome frame";
    case BrowserDistribution::CHROME_BINARIES:
      return "chrome binaries";
    case BrowserDistribution::NUM_TYPES:
      // Fall out of switch.
      break;
  }
  NOTREACHED();
  return "";
}

const char *OperationToString(InstallerState::Operation operation) {
  switch (operation) {
    case InstallerState::SINGLE_INSTALL_OR_UPDATE:
      return "single-install-or-update";
    case InstallerState::MULTI_INSTALL:
      return "multi-install";
    case InstallerState::MULTI_UPDATE:
      return "multi-update";
    case InstallerState::UNINSTALL:
      return "uninstall";
    case InstallerState::UNINITIALIZED:
      // Fall out of switch.
      break;
  }
  NOTREACHED();
  return "";
}

}  // namespace

void ConfigureCrashReporting(const InstallerState& installer_state) {
  // This is inspired by work done in various parts of Chrome startup to connect
  // to the crash service. Since the installer does not split its work between
  // a stub .exe and a main .dll, crash reporting can be configured in one place
  // right here.

  // Create the crash client and install it (a la MainDllLoader::Launch).
  InstallerCrashReporterClient *crash_client =
      new InstallerCrashReporterClient(!installer_state.system_install());
  ANNOTATE_LEAKING_OBJECT_PTR(crash_client);
  crash_reporter::SetCrashReporterClient(crash_client);

  breakpad::InitCrashReporter("Chrome Installer");

  // Set up crash keys and the client id (a la child_process_logging::Init()).
#if defined(COMPONENT_BUILD)
  // breakpad::InitCrashReporter takes care of this for static builds but not
  // component builds due to intricacies of chrome.exe and chrome.dll sharing a
  // copy of base.dll in that case (for details, see the comment in
  // components/crash/content/app/breakpad_win.cc).
  crash_client->RegisterCrashKeys();
  base::debug::SetCrashKeyReportingFunctions(&SetCrashKeyValue, &ClearCrashKey);
#endif // COMPONENT_BUILD

  scoped_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info)
    crash_client->SetCrashReporterClientIdFromGUID(client_info->client_id);
  // TODO(grt): A lack of a client_id at this point generally means that Chrome
  // has yet to have been launched and picked one. Consider creating it and
  // setting it here for Chrome to use.
}

size_t RegisterCrashKeys() {
  const base::debug::CrashKey kFixedKeys[] = {
    { crash_keys::kClientId, crash_keys::kSmallSize },
    { kDistributionType, crash_keys::kSmallSize },
    { kIsMultiInstall, crash_keys::kSmallSize },
    { kIsSystemLevel, crash_keys::kSmallSize },
    { kOperation, crash_keys::kSmallSize },

    // This is a Windows registry key, which maxes out at 255 chars.
    // (kMediumSize actually maxes out at 252 chars on Windows, but potentially
    // truncating such a small amount is a fair tradeoff compared to using
    // kLargeSize, which is wasteful.)
    { kStateKey, crash_keys::kMediumSize },
  };
  return base::debug::InitCrashKeys(&kFixedKeys[0], arraysize(kFixedKeys),
                                    crash_keys::kChunkMaxLength);
}

void SetInitialCrashKeys(const InstallerState& state) {
  using base::debug::SetCrashKeyValue;

  SetCrashKeyValue(kDistributionType,
                   DistributionTypeToString(state.state_type()));
  SetCrashKeyValue(kOperation, OperationToString(state.operation()));
  SetCrashKeyValue(kIsMultiInstall,
                   state.is_multi_install() ? "true" : "false");
  SetCrashKeyValue(kIsSystemLevel, state.system_install() ? "true" : "false");

  const base::string16 state_key = state.state_key();
  if (!state_key.empty())
    SetCrashKeyValue(kStateKey, base::UTF16ToUTF8(state_key));
}

}  // namespace installer
