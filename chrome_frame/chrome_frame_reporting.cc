// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of wrapper around common crash reporting.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome_frame/chrome_frame_reporting.h"

// Well known SID for the system principal.
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

// Returns the custom info structure based on the dll in parameter and the
// process type.
google_breakpad::CustomClientInfo* GetCustomInfo() {
  // TODO(joshia): Grab these based on build.
  static google_breakpad::CustomInfoEntry ver_entry(L"ver", L"0.1.0.0");
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", L"ChromeFrame");
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", L"chrome_frame");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, arraysize(entries) };
  return &custom_info;
}

extern "C" IMAGE_DOS_HEADER __ImageBase;

bool InitializeCrashReporting() {
  // We want to use the Google Update crash reporting. We need to check if the
  // user allows it first.
  if (!GoogleUpdateSettings::GetCollectStatsConsent())
    return true;

  // Build the pipe name. It can be either:
  // System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
  // Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
  wchar_t dll_path[MAX_PATH * 2] = {0};
  GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase), dll_path,
                    arraysize(dll_path));

  std::wstring user_sid;
  if (InstallUtil::IsPerUserInstall(dll_path)) {
    if (!win_util::GetUserSidString(&user_sid)) {
      return false;
    }
  } else {
    user_sid = kSystemPrincipalSid;
  }

  // Get the alternate dump directory. We use the temp path.
  FilePath temp_directory;
  if (!file_util::GetTempDir(&temp_directory) || temp_directory.empty()) {
    return false;
  }

  return InitializeVectoredCrashReporting(false, user_sid.c_str(),
      temp_directory.value(), GetCustomInfo());
}

bool ShutdownCrashReporting() {
  return ShutdownVectoredCrashReporting();
}
