// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/hardcoded_blocklist.h"

#include <windows.h>

namespace third_party_dlls {

namespace {

// Utility function for converting UTF-8 to UTF-16.
// Note: Not using base::UTF8ToUTF16() because chrome_elf can not have any
//       dependencies on //base.
bool UTF8ToUTF16(const std::string& utf8, std::wstring* utf16) {
  assert(utf16);

  if (utf8.empty()) {
    utf16->clear();
    return true;
  }

  int size_needed_chars = ::MultiByteToWideChar(
      CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
  if (!size_needed_chars)
    return false;

  utf16->resize(size_needed_chars);
  return ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                               static_cast<int>(utf8.size()), &(*utf16)[0],
                               size_needed_chars);
}

}  // namespace

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the browser. DLLs should only be added to this list
// if there is nothing else Chrome can do to prevent those crashes.
// For more information about how this list is generated, and how to get off
// of it, see:
// https://sites.google.com/a/chromium.org/dev/Home/third-party-developers
// NOTE: Please remember to update the DllHash enum in histograms.xml when
//       adding a new value to the blacklist.
// TODO(pmonette): Change to const char* const when the
//                 chrome/chrome_elf/blacklist directory gets deleted.
const wchar_t* g_troublesome_dlls[kTroublesomeDllsMaxCount] = {
    L"949ba8b6a9.dll",           // Coupon Time.
    L"activedetect32.dll",       // Lenovo One Key Theater.
                                 // See crbug.com/379218.
    L"activedetect64.dll",       // Lenovo One Key Theater.
    L"bitguard.dll",             // Unknown (suspected malware).
    L"bsvc.dll",                 // Unknown (suspected adware).
    L"chrmxtn.dll",              // Unknown (keystroke logger).
    L"cplushook.dll",            // Unknown (suspected malware).
    L"crdli.dll",                // Linkury Inc.
    L"crdli64.dll",              // Linkury Inc.
    L"datamngr.dll",             // Unknown (suspected adware).
    L"dpinterface32.dll",        // Unknown (suspected adware).
    L"explorerex.dll",           // Unknown (suspected adware).
    L"hk.dll",                   // Unknown (keystroke logger).
    L"libapi2hook.dll",          // V-Bates.
    L"libinject.dll",            // V-Bates.
    L"libinject2.dll",           // V-Bates.
    L"libredir2.dll",            // V-Bates.
    L"libsvn_tsvn32.dll",        // TortoiseSVN.
    L"libwinhook.dll",           // V-Bates.
    L"lmrn.dll",                 // Unknown.
    L"minisp.dll",               // Unknown (suspected malware).
    L"minisp32.dll",             // Unknown (suspected malware).
    L"offerswizarddll.dll",      // Unknown (suspected adware).
    L"safetynut.dll",            // Unknown (suspected adware).
    L"smdmf.dll",                // Unknown (suspected adware).
    L"spappsv32.dll",            // Unknown (suspected adware).
    L"systemk.dll",              // Unknown (suspected adware).
    L"vntsrv.dll",               // Virtual New Tab by APN LLC.
    L"wajam_goblin_64.dll",      // Wajam Internet Technologies.
    L"wajam_goblin.dll",         // Wajam Internet Technologies.
    L"windowsapihookdll32.dll",  // Lenovo One Key Theater.
                                 // See crbug.com/379218.
    L"windowsapihookdll64.dll",  // Lenovo One Key Theater.
    L"virtualcamera.ax",         // %PROGRAMFILES%\ASUS\VirtualCamera.
                                 // See crbug.com/422522.
    L"ycwebcamerasource.ax",     // CyberLink Youcam, crbug.com/424159
    // Keep this null pointer here to mark the end of the list.
    nullptr,
};

bool DllMatch(const std::string& module_name) {
  if (module_name.empty())
    return false;

  // Convert UTF-8 to UTF-16 for this comparison.
  std::wstring wide_module_name;
  if (!UTF8ToUTF16(module_name, &wide_module_name))
    return false;

  for (int i = 0; g_troublesome_dlls[i] != nullptr; ++i) {
    if (_wcsicmp(wide_module_name.c_str(), g_troublesome_dlls[i]) == 0)
      return i;
  }

  return false;
}

}  // namespace third_party_dlls
