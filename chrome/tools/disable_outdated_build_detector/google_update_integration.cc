// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/google_update_integration.h"

#include <stdlib.h>

#include <algorithm>

uint32_t OpenClientStateKey(bool system_level, App app, HKEY* key) {
#if defined(GOOGLE_CHROME_BUILD)
  constexpr wchar_t kChromeAppGuid[] =
      L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
  constexpr wchar_t kBinariesAppGuid[] =
      L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";
  // presubmit: allow wstring
  std::wstring path(L"Software\\Google\\Update\\ClientState\\");
  path += (app == App::CHROME_BINARIES ? kBinariesAppGuid : kChromeAppGuid);
#else
  // presubmit: allow wstring
  std::wstring path(app == App::CHROME_BINARIES ? L"Software\\Chromium Binaries"
                                                : L"Software\\Chromium");
#endif
  return ::RegOpenKeyEx(system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                        path.c_str(), 0 /* ulOptions */,
                        KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY, key);
}

void WriteResultInfo(bool system_level, const ResultInfo& result_info) {
  HKEY key = nullptr;
  uint32_t result = OpenClientStateKey(system_level, App::CHROME_BROWSER, &key);
  if (result != ERROR_SUCCESS)
    return;
  ::RegSetValueEx(
      key, kInstallerResult, 0 /* Reserved */, REG_DWORD,
      reinterpret_cast<const uint8_t*>(&result_info.installer_result),
      sizeof(DWORD));
  ::RegSetValueEx(
      key, kInstallerError, 0 /* Reserved */, REG_DWORD,
      reinterpret_cast<const uint8_t*>(&result_info.installer_error),
      sizeof(DWORD));
  if (result_info.installer_extra_code1) {
    ::RegSetValueEx(
        key, kInstallerExtraCode1, 0 /* Reserved */, REG_DWORD,
        reinterpret_cast<const uint8_t*>(&result_info.installer_extra_code1),
        sizeof(DWORD));
  } else {
    ::RegDeleteValue(key, kInstallerExtraCode1);
  }
  ::RegCloseKey(key);
}

// Copied from chrome/browser/google/google_brand.cc.
// presubmit: allow wstring
bool IsOrganic(const std::wstring& brand) {
  constexpr const wchar_t* kBrands[] = {
      L"CHCA", L"CHCB", L"CHCG", L"CHCH", L"CHCI", L"CHCJ", L"CHCK", L"CHCL",
      L"CHFO", L"CHFT", L"CHHS", L"CHHM", L"CHMA", L"CHMB", L"CHME", L"CHMF",
      L"CHMG", L"CHMH", L"CHMI", L"CHMQ", L"CHMV", L"CHNB", L"CHNC", L"CHNG",
      L"CHNH", L"CHNI", L"CHOA", L"CHOB", L"CHOC", L"CHON", L"CHOO", L"CHOP",
      L"CHOQ", L"CHOR", L"CHOS", L"CHOT", L"CHOU", L"CHOX", L"CHOY", L"CHOZ",
      L"CHPD", L"CHPE", L"CHPF", L"CHPG", L"ECBA", L"ECBB", L"ECDA", L"ECDB",
      L"ECSA", L"ECSB", L"ECVA", L"ECVB", L"ECWA", L"ECWB", L"ECWC", L"ECWD",
      L"ECWE", L"ECWF", L"EUBB", L"EUBC", L"GGLA", L"GGLS"};
  const wchar_t* const* end = &kBrands[_countof(kBrands)];
  const wchar_t* const* found = std::find(&kBrands[0], end, brand);
  if (found != end)
    return true;

  return brand.size() > 3 && (std::equal(&brand[0], &brand[3], L"EUB") ||
                              std::equal(&brand[0], &brand[3], L"EUC") ||
                              std::equal(&brand[0], &brand[3], L"GGR"));
}
