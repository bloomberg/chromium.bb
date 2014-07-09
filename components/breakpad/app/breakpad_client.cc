// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breakpad/app/breakpad_client.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace breakpad {

namespace {

BreakpadClient* g_client = NULL;

}  // namespace

void SetBreakpadClient(BreakpadClient* client) {
  g_client = client;
}

BreakpadClient* GetBreakpadClient() {
  DCHECK(g_client);
  return g_client;
}

BreakpadClient::BreakpadClient() {}
BreakpadClient::~BreakpadClient() {}

void BreakpadClient::SetBreakpadClientIdFromGUID(
    const std::string& client_guid) {
}

#if defined(OS_WIN)
bool BreakpadClient::GetAlternativeCrashDumpLocation(
    base::FilePath* crash_dir) {
  return false;
}

void BreakpadClient::GetProductNameAndVersion(const base::FilePath& exe_path,
                                              base::string16* product_name,
                                              base::string16* version,
                                              base::string16* special_build,
                                              base::string16* channel_name) {
}

bool BreakpadClient::ShouldShowRestartDialog(base::string16* title,
                                             base::string16* message,
                                             bool* is_rtl_locale) {
  return false;
}

bool BreakpadClient::AboutToRestart() {
  return false;
}

bool BreakpadClient::GetDeferredUploadsSupported(bool is_per_usr_install) {
  return false;
}

bool BreakpadClient::GetIsPerUserInstall(const base::FilePath& exe_path) {
  return true;
}

bool BreakpadClient::GetShouldDumpLargerDumps(bool is_per_user_install) {
  return false;
}

int BreakpadClient::GetResultCodeRespawnFailed() {
  return 0;
}

void BreakpadClient::InitBrowserCrashDumpsRegKey() {
}

void BreakpadClient::RecordCrashDumpAttempt(bool is_real_crash) {
}
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
void BreakpadClient::GetProductNameAndVersion(std::string* product_name,
                                              std::string* version) {
}

base::FilePath BreakpadClient::GetReporterLogFilename() {
  return base::FilePath();
}
#endif

bool BreakpadClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  return false;
}

size_t BreakpadClient::RegisterCrashKeys() {
  return 0;
}

bool BreakpadClient::IsRunningUnattended() {
  return true;
}

bool BreakpadClient::GetCollectStatsConsent() {
  return false;
}

#if defined(OS_WIN) || defined(OS_MACOSX)
bool BreakpadClient::ReportingIsEnforcedByPolicy(bool* breakpad_enabled) {
  return false;
}
#endif

#if defined(OS_ANDROID)
int BreakpadClient::GetAndroidMinidumpDescriptor() {
  return 0;
}
#endif

#if defined(OS_MACOSX)
void BreakpadClient::InstallAdditionalFilters(BreakpadRef breakpad) {
}
#endif

bool BreakpadClient::EnableBreakpadForProcess(const std::string& process_type) {
  return false;
}

}  // namespace breakpad
