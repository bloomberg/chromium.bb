// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_crash_reporter_client.h"

#include "base/debug/crash_logging.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/util/google_update_settings.h"

InstallerCrashReporterClient::InstallerCrashReporterClient(
    bool is_per_user_install)
    : is_per_user_install_(is_per_user_install) {
}

InstallerCrashReporterClient::~InstallerCrashReporterClient() = default;

bool InstallerCrashReporterClient::ShouldCreatePipeName(
    const base::string16& process_type) {
  return true;
}

bool InstallerCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::FilePath* crash_dir) {
  return false;
}

void InstallerCrashReporterClient::GetProductNameAndVersion(
    const base::FilePath& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  // Report crashes under the same product name as the browser. This string
  // MUST match server-side configuration.
  *product_name = base::ASCIIToUTF16(PRODUCT_SHORTNAME_STRING);

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(exe_path));
  if (version_info) {
    *version = version_info->product_version();
    *special_build = version_info->special_build();
  } else {
    *version = L"0.0.0.0-devel";
  }

  GoogleUpdateSettings::GetChromeChannelAndModifiers(
      !GetIsPerUserInstall(exe_path), channel_name);
}

bool InstallerCrashReporterClient::ShouldShowRestartDialog(
    base::string16* title,
    base::string16* message,
    bool* is_rtl_locale) {
  // There is no UX associated with the installer, so no dialog should be shown.
  return false;
}

bool InstallerCrashReporterClient::AboutToRestart() {
  // The installer should never be restarted after a crash.
  return false;
}

bool InstallerCrashReporterClient::GetDeferredUploadsSupported(
    bool is_per_user_install) {
  // Copy Chrome's impl?
  return false;
}

bool InstallerCrashReporterClient::GetIsPerUserInstall(
    const base::FilePath& exe_path) {
  return is_per_user_install_;
}

bool InstallerCrashReporterClient::GetShouldDumpLargerDumps(
    bool is_per_user_install) {
  DCHECK_EQ(is_per_user_install_, is_per_user_install);
  base::string16 channel =
      GoogleUpdateSettings::GetChromeChannel(!is_per_user_install);
  // Use large dumps for all but the stable channel.
  return !channel.empty();
}

int InstallerCrashReporterClient::GetResultCodeRespawnFailed() {
  // The restart dialog is never shown for the installer.
  NOTREACHED();
  return 0;
}

void InstallerCrashReporterClient::InitBrowserCrashDumpsRegKey() {
  // The installer does not track dump attempts and results in the registry.
}

void InstallerCrashReporterClient::RecordCrashDumpAttempt(bool is_real_crash) {
  // The installer does not track dump attempts and results in the registry.
}

void InstallerCrashReporterClient::RecordCrashDumpAttemptResult(
    bool is_real_crash,
    bool succeeded) {
  // The installer does not track dump attempts and results in the registry.
}

bool InstallerCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  return PathService::Get(chrome::DIR_CRASH_DUMPS, crash_dir);
}

size_t InstallerCrashReporterClient::RegisterCrashKeys() {
  return installer::RegisterCrashKeys();
}

bool InstallerCrashReporterClient::IsRunningUnattended() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar(env_vars::kHeadless);
}

bool InstallerCrashReporterClient::GetCollectStatsConsent() {
#if defined(GOOGLE_CHROME_BUILD)
  return GoogleUpdateSettings::GetCollectStatsConsentAtLevel(
      !is_per_user_install_);
#else
  return false;
#endif
}

bool InstallerCrashReporterClient::ReportingIsEnforcedByPolicy(bool* enabled) {
  // From the generated policy/policy/policy_constants.cc:
#if defined(GOOGLE_CHROME_BUILD)
  static const wchar_t kRegistryChromePolicyKey[] =
      L"SOFTWARE\\Policies\\Google\\Chrome";
#else
  static const wchar_t kRegistryChromePolicyKey[] =
      L"SOFTWARE\\Policies\\Chromium";
#endif
  static const wchar_t kMetricsReportingEnabled[] = L"MetricsReportingEnabled";

  // Determine whether configuration management allows loading the crash
  // reporter. Since the configuration management infrastructure is not
  // initialized in the installer, the corresponding registry keys are read
  // directly. The return status indicates whether policy data was successfully
  // read. If it is true, |enabled| contains the value set by policy.
  DWORD value = 0;
  base::win::RegKey policy_key;
  static const HKEY kHives[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (HKEY hive : kHives) {
    if (policy_key.Open(hive, kRegistryChromePolicyKey,
                        KEY_READ) == ERROR_SUCCESS &&
        policy_key.ReadValueDW(kMetricsReportingEnabled,
                               &value) == ERROR_SUCCESS) {
      *enabled = value != 0;
      return true;
    }
  }

  return false;
}

bool InstallerCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return true;
}
