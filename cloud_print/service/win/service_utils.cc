// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/service_utils.h"

#include <windows.h>
#include <security.h>  // NOLINT

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_switches.h"

base::string16 GetLocalComputerName() {
  DWORD size = 0;
  base::string16 result;
  ::GetComputerName(NULL, &size);
  result.resize(size);
  if (result.empty())
    return result;
  if (!::GetComputerName(&result[0], &size))
    return base::string16();
  result.resize(size);
  return result;
}

base::string16 ReplaceLocalHostInName(const base::string16& user_name) {
  static const wchar_t kLocalDomain[] = L".\\";
  if (base::StartsWith(user_name, kLocalDomain,
                       base::CompareCase::SENSITIVE)) {
    return GetLocalComputerName() +
           user_name.substr(arraysize(kLocalDomain) - 2);
  }
  return user_name;
}

base::string16 GetCurrentUserName() {
  ULONG size = 0;
  base::string16 result;
  ::GetUserNameEx(::NameSamCompatible, NULL, &size);
  result.resize(size);
  if (result.empty())
    return result;
  if (!::GetUserNameEx(::NameSamCompatible, &result[0], &size))
    return base::string16();
  result.resize(size);
  return result;
}

void CopyChromeSwitchesFromCurrentProcess(base::CommandLine* destination) {
  static const char* const kSwitchesToCopy[] = {
    switches::kCloudPrintURL,
    switches::kCloudPrintXmppEndpoint,
    switches::kEnableCloudPrintXps,
    switches::kEnableLogging,
    switches::kIgnoreUrlFetcherCertRequests,
    switches::kLsoUrl,
    switches::kV,
  };
  destination->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy,
                                arraysize(kSwitchesToCopy));
}

