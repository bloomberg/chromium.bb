// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_crash_reporting.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/setup/installer_crash_reporter_client.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/crash/content/app/crash_keys_win.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_keys.h"

namespace installer {

namespace {

// Crash Keys

const char kCurrentVersion[] = "current-version";
const char kIsSystemLevel[] = "system-level";
const char kOperation[] = "operation";
const char kStateKey[] = "state-key";

const char *OperationToString(InstallerState::Operation operation) {
  switch (operation) {
    case InstallerState::SINGLE_INSTALL_OR_UPDATE:
      return "single-install-or-update";
    case InstallerState::UNINSTALL:
      return "uninstall";
    case InstallerState::UNINITIALIZED:
      // Fall out of switch.
      break;
  }
  NOTREACHED();
  return "";
}

// Retrieve the SYSTEM version of TEMP. We do this instead of GetTempPath so
// that both elevated and SYSTEM runs share the same directory.
bool GetSystemTemp(base::FilePath* temp) {
  base::win::RegKey reg_key(
      HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
      KEY_READ);
  std::wstring temp_wstring;  // presubmit: allow wstring
  bool success = reg_key.ReadValue(L"TEMP", &temp_wstring) == ERROR_SUCCESS;
  if (success)
    *temp = base::FilePath(temp_wstring);  // presubmit: allow wstring
  return success;
}

}  // namespace

void ConfigureCrashReporting(const InstallerState& installer_state) {
  // This is inspired by work done in various parts of Chrome startup to connect
  // to the crash service. Since the installer does not split its work between
  // a stub .exe and a main .dll, crash reporting can be configured in one place
  // right here.

  // Create the crash client and install it (a la MainDllLoader::Launch).
  InstallerCrashReporterClient* crash_client =
      new InstallerCrashReporterClient(!installer_state.system_install());
  ANNOTATE_LEAKING_OBJECT_PTR(crash_client);
  crash_reporter::SetCrashReporterClient(crash_client);

  if (installer_state.system_install()) {
    base::FilePath temp_dir;
    if (GetSystemTemp(&temp_dir)) {
      base::FilePath crash_dir = temp_dir.Append(FILE_PATH_LITERAL("Crashpad"));
      PathService::OverrideAndCreateIfNeeded(chrome::DIR_CRASH_DUMPS, crash_dir,
                                             true, true);
    } else {
      // Failed to get a temp dir, something's gone wrong.
      return;
    }
  }

  crash_reporter::InitializeCrashpadWithEmbeddedHandler(true,
                                                        "Chrome Installer");

  // Set up the metrics client id (a la child_process_logging::Init()).
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info)
    crash_keys::SetMetricsClientIdFromGUID(client_info->client_id);
}

size_t RegisterCrashKeys() {
  const base::debug::CrashKey kFixedKeys[] = {
    { crash_keys::kMetricsClientId, crash_keys::kSmallSize },
    { kCurrentVersion, crash_keys::kSmallSize },
    { kIsSystemLevel, crash_keys::kSmallSize },
    { kOperation, crash_keys::kSmallSize },

    // This is a Windows registry key, which maxes out at 255 chars.
    // (kMediumSize actually maxes out at 252 chars on Windows, but potentially
    // truncating such a small amount is a fair tradeoff compared to using
    // kLargeSize, which is wasteful.)
    { kStateKey, crash_keys::kMediumSize },
  };
  std::vector<base::debug::CrashKey> keys(std::begin(kFixedKeys),
                                          std::end(kFixedKeys));
  crash_keys::GetCrashKeysForCommandLineSwitches(&keys);
  return base::debug::InitCrashKeys(keys.data(), keys.size(),
                                    crash_keys::kChunkMaxLength);
}

void SetInitialCrashKeys(const InstallerState& state) {
  using base::debug::SetCrashKeyValue;

  SetCrashKeyValue(kOperation, OperationToString(state.operation()));
  SetCrashKeyValue(kIsSystemLevel, state.system_install() ? "true" : "false");

  const base::string16 state_key = state.state_key();
  if (!state_key.empty())
    SetCrashKeyValue(kStateKey, base::UTF16ToUTF8(state_key));
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
}

void SetCurrentVersionCrashKey(const base::Version* current_version) {
  if (current_version) {
    base::debug::SetCrashKeyValue(kCurrentVersion,
                                  current_version->GetString());
  } else {
    base::debug::ClearCrashKey(kCurrentVersion);
  }
}

}  // namespace installer
