// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/google_update_integration.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"

uint32_t OpenClientStateKey(bool system_level,
                            const wchar_t* app_guid,
                            base::win::RegKey* key) {
#if defined(GOOGLE_CHROME_BUILD)
  base::string16 path(base::StringPrintf(
      L"Software\\Google\\Update\\ClientState\\%ls", app_guid));
#else
  base::string16 path(app_guid == kBinariesAppGuid
                          ? L"Software\\Chromium Binaries"
                          : L"Software\\Chromium");
#endif
  return key->Open(system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                   path.c_str(),
                   KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
}

void WriteResultInfo(bool system_level, const ResultInfo& result_info) {
  base::win::RegKey key;
  uint32_t result = OpenClientStateKey(system_level, kChromeAppGuid, &key);
  if (result != ERROR_SUCCESS)
    return;
  key.WriteValue(kInstallerResult,
                 static_cast<uint32_t>(result_info.installer_result));
  key.WriteValue(kInstallerError,
                 static_cast<uint32_t>(result_info.installer_error));
  if (result_info.installer_extra_code1) {
    key.WriteValue(kInstallerExtraCode1,
                   static_cast<uint32_t>(result_info.installer_error));
  } else {
    key.DeleteValue(kInstallerExtraCode1);
  }
}

// Copied from chrome/browser/google/google_brand.cc.
bool IsOrganic(const base::string16& brand) {
  static const wchar_t* const kBrands[] = {
      L"CHCA", L"CHCB", L"CHCG", L"CHCH", L"CHCI", L"CHCJ", L"CHCK", L"CHCL",
      L"CHFO", L"CHFT", L"CHHS", L"CHHM", L"CHMA", L"CHMB", L"CHME", L"CHMF",
      L"CHMG", L"CHMH", L"CHMI", L"CHMQ", L"CHMV", L"CHNB", L"CHNC", L"CHNG",
      L"CHNH", L"CHNI", L"CHOA", L"CHOB", L"CHOC", L"CHON", L"CHOO", L"CHOP",
      L"CHOQ", L"CHOR", L"CHOS", L"CHOT", L"CHOU", L"CHOX", L"CHOY", L"CHOZ",
      L"CHPD", L"CHPE", L"CHPF", L"CHPG", L"ECBA", L"ECBB", L"ECDA", L"ECDB",
      L"ECSA", L"ECSB", L"ECVA", L"ECVB", L"ECWA", L"ECWB", L"ECWC", L"ECWD",
      L"ECWE", L"ECWF", L"EUBB", L"EUBC", L"GGLA", L"GGLS"};
  const wchar_t* const* end = &kBrands[arraysize(kBrands)];
  const wchar_t* const* found = std::find(&kBrands[0], end, brand);
  if (found != end)
    return true;

  return base::StartsWith(brand, L"EUB", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, L"EUC", base::CompareCase::SENSITIVE) ||
         base::StartsWith(brand, L"GGR", base::CompareCase::SENSITIVE);
}
