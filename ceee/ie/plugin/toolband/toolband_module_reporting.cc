// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of CEEE plugin's wrapper around common crash reporting.

#include "ceee/ie/plugin/toolband/toolband_module_reporting.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringize_macros.h"
#include "ceee/ie/common/ceee_module_util.h"

#include "version.h"  // NOLINT

// Well known SID for the system principal.
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

// Returns the custom info structure based on the dll in parameter and the
// process type.
google_breakpad::CustomClientInfo* GetCustomInfo() {
  static google_breakpad::CustomInfoEntry ver_entry(
      L"ver", TO_L_STRING(CHROME_VERSION_STRING));
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", L"CEEE_IE");
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", L"ie_plugin");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, arraysize(entries) };
  return &custom_info;
}

bool InitializeCrashReporting() {
  if (!ceee_module_util::GetCollectStatsConsent())
    return false;

  // Get the alternate dump directory. We use the temp path.
  FilePath temp_directory;
  if (!file_util::GetTempDir(&temp_directory) || temp_directory.empty()) {
    return false;
  }

  bool result = InitializeVectoredCrashReporting(
      false, kSystemPrincipalSid, temp_directory.value(), GetCustomInfo());
  DCHECK(result) << "Failed initialize crashreporting.";
  return result;
}

bool ShutdownCrashReporting() {
  return ShutdownVectoredCrashReporting();
}
