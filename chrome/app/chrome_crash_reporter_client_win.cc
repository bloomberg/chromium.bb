// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(ananta/scottmg)
// Add test coverage for Crashpad.
#include "chrome/app/chrome_crash_reporter_client_win.h"

#include <windows.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/install_static/install_util.h"
#include "content/public/common/content_switches.h"

namespace {

// This is the minimum version of google update that is required for deferred
// crash uploads to work.
const char kMinUpdateVersion[] = "1.3.21.115";

}  // namespace

ChromeCrashReporterClient::ChromeCrashReporterClient() {}

ChromeCrashReporterClient::~ChromeCrashReporterClient() {}

bool ChromeCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  std::string alternate_crash_dump_location =
      install_static::GetEnvironmentString("BREAKPAD_DUMP_LOCATION");
  if (!alternate_crash_dump_location.empty()) {
    *crash_dir = base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
    return true;
  }
  return false;
}

void ChromeCrashReporterClient::GetProductNameAndVersion(
    const base::FilePath& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  DCHECK(product_name);
  DCHECK(version);
  DCHECK(special_build);
  DCHECK(channel_name);

  install_static::GetExecutableVersionDetails(
      exe_path.value(), product_name, version, special_build, channel_name);
}

bool ChromeCrashReporterClient::ShouldShowRestartDialog(base::string16* title,
                                                        base::string16* message,
                                                        bool* is_rtl_locale) {
  if (!install_static::HasEnvironmentVariable(env_vars::kShowRestart) ||
      !install_static::HasEnvironmentVariable(env_vars::kRestartInfo) ||
      install_static::HasEnvironmentVariable(env_vars::kMetroConnected)) {
    return false;
  }

  std::string restart_info =
      install_static::GetEnvironmentString(env_vars::kRestartInfo);

  // The CHROME_RESTART var contains the dialog strings separated by '|'.
  // See ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment()
  // for details.
  std::vector<std::string> dlg_strings = base::SplitString(
      restart_info, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (dlg_strings.size() < 3)
    return false;

  *title = base::UTF8ToUTF16(dlg_strings[0]);
  *message = base::UTF8ToUTF16(dlg_strings[1]);
  *is_rtl_locale = dlg_strings[2] == env_vars::kRtlLocale;
  return true;
}

bool ChromeCrashReporterClient::AboutToRestart() {
  if (!install_static::HasEnvironmentVariable(env_vars::kRestartInfo))
    return false;

  install_static::SetEnvironmentString(env_vars::kShowRestart, "1");
  return true;
}

bool ChromeCrashReporterClient::GetDeferredUploadsSupported(
    bool is_per_user_install) {
  std::string version_string = install_static::GetGoogleUpdateVersion();
  Version update_version(version_string);
  if (!update_version.IsValid() ||
      update_version < base::Version(kMinUpdateVersion)) {
    return false;
  }
  return true;
}

bool ChromeCrashReporterClient::GetIsPerUserInstall(
    const base::FilePath& exe_path) {
  return !install_static::IsSystemInstall(exe_path.value().c_str());
}

bool ChromeCrashReporterClient::GetShouldDumpLargerDumps(
    bool is_per_user_install) {
  base::string16 channel_name;
  install_static::GetChromeChannelName(is_per_user_install,
                                       false, // !add_modifier
                                       &channel_name);
  // Capture more detail in crash dumps for Beta, Dev, Canary channels and
  // if channel is unknown (e.g. Chromium or developer builds).
  return (channel_name == install_static::kChromeChannelBeta ||
          channel_name == install_static::kChromeChannelDev ||
          channel_name == install_static::kChromeChannelCanary ||
          channel_name == install_static::kChromeChannelUnknown);
}

int ChromeCrashReporterClient::GetResultCodeRespawnFailed() {
  return chrome::RESULT_CODE_RESPAWN_FAILED;
}

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* breakpad_enabled) {
  // Determine whether configuration management allows loading the crash
  // reporter.
  // Since the configuration management infrastructure is not initialized at
  // this point, we read the corresponding registry key directly. The return
  // status indicates whether policy data was successfully read. If it is true,
  // |breakpad_enabled| contains the value set by policy.
  return install_static::ReportingIsEnforcedByPolicy(breakpad_enabled);
}


bool ChromeCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  // If this environment variable exists, then for the time being,
  // short-circuit how it's handled on Windows. Honoring this
  // variable is required in order to symbolize stack traces in
  // Telemetry based tests: http://crbug.com/561763.
  if (GetAlternativeCrashDumpLocation(crash_dir))
    return true;

  // TODO(scottmg): Consider supporting --user-data-dir. See
  // https://crbug.com/565446.
  base::string16 crash_dump_location;
  if (!install_static::GetDefaultCrashDumpLocation(&crash_dump_location))
    return false;
  *crash_dir = base::FilePath(crash_dump_location);
  return true;
}

size_t ChromeCrashReporterClient::RegisterCrashKeys() {
  return crash_keys::RegisterChromeCrashKeys();
}

bool ChromeCrashReporterClient::IsRunningUnattended() {
  return install_static::HasEnvironmentVariable(env_vars::kHeadless);
}

bool ChromeCrashReporterClient::GetCollectStatsConsent() {
  return install_static::GetCollectStatsConsent();
}

bool ChromeCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}
