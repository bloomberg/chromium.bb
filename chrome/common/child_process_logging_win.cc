// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <windows.h>

#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/common/gpu_info.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {
// exported in breakpad_win.cc: void __declspec(dllexport) __cdecl SetActiveURL.
typedef void (__cdecl *MainSetActiveURL)(const wchar_t*);

// exported in breakpad_win.cc: void __declspec(dllexport) __cdecl SetClientId.
typedef void (__cdecl *MainSetClientId)(const wchar_t*);

// exported in breakpad_win.cc:
//   void __declspec(dllexport) __cdecl SetNumberOfExtensions.
typedef void (__cdecl *MainSetNumberOfExtensions)(int);

// exported in breakpad_win.cc:
// void __declspec(dllexport) __cdecl SetExtensionID.
typedef void (__cdecl *MainSetExtensionID)(size_t, const wchar_t*);

// exported in breakpad_win.cc: void __declspec(dllexport) __cdecl SetGpuInfo.
typedef void (__cdecl *MainSetGpuInfo)(const wchar_t*, const wchar_t*,
                                       const wchar_t*, const wchar_t*,
                                       const wchar_t*);

// exported in breakpad_win.cc:
//   void __declspec(dllexport) __cdecl SetNumberOfViews.
typedef void (__cdecl *MainSetNumberOfViews)(int);

void SetActiveURL(const GURL& url) {
  static MainSetActiveURL set_active_url = NULL;
  // note: benign race condition on set_active_url.
  if (!set_active_url) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_active_url = reinterpret_cast<MainSetActiveURL>(
        GetProcAddress(exe_module, "SetActiveURL"));
    if (!set_active_url)
      return;
  }

  (set_active_url)(UTF8ToWide(url.possibly_invalid_spec()).c_str());
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  if (str.empty())
    return;

  std::wstring wstr = ASCIIToWide(str);
  std::wstring old_wstr;
  if (!GoogleUpdateSettings::GetMetricsId(&old_wstr) ||
      wstr != old_wstr)
    GoogleUpdateSettings::SetMetricsId(wstr);

  static MainSetClientId set_client_id = NULL;
  if (!set_client_id) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_client_id = reinterpret_cast<MainSetClientId>(
        GetProcAddress(exe_module, "SetClientId"));
    if (!set_client_id)
      return;
  }
  (set_client_id)(wstr.c_str());
}

std::string GetClientId() {
  std::wstring wstr_client_id;
  if (GoogleUpdateSettings::GetMetricsId(&wstr_client_id))
    return WideToASCII(wstr_client_id);
  else
    return std::string();
}

void SetActiveExtensions(const std::set<std::string>& extension_ids) {
  static MainSetNumberOfExtensions set_number_of_extensions = NULL;
  if (!set_number_of_extensions) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_number_of_extensions = reinterpret_cast<MainSetNumberOfExtensions>(
        GetProcAddress(exe_module, "SetNumberOfExtensions"));
    if (!set_number_of_extensions)
      return;
  }

  static MainSetExtensionID set_extension_id = NULL;
  if (!set_extension_id) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_extension_id = reinterpret_cast<MainSetExtensionID>(
        GetProcAddress(exe_module, "SetExtensionID"));
    if (!set_extension_id)
      return;
  }

  (set_number_of_extensions)(static_cast<int>(extension_ids.size()));

  std::set<std::string>::const_iterator iter = extension_ids.begin();
  for (size_t i = 0; i < kMaxReportedActiveExtensions; ++i) {
    if (iter != extension_ids.end()) {
      (set_extension_id)(i, ASCIIToWide(iter->c_str()).c_str());
      ++iter;
    } else {
      (set_extension_id)(i, L"");
    }
  }
}

void SetGpuInfo(const GPUInfo& gpu_info) {
  static MainSetGpuInfo set_gpu_info = NULL;
  if (!set_gpu_info) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_gpu_info = reinterpret_cast<MainSetGpuInfo>(
        GetProcAddress(exe_module, "SetGpuInfo"));
    if (!set_gpu_info)
      return;
  }
  (set_gpu_info)(
      base::StringPrintf(L"0x%04x", gpu_info.vendor_id).c_str(),
      base::StringPrintf(L"0x%04x", gpu_info.device_id).c_str(),
      UTF8ToUTF16(gpu_info.driver_version).c_str(),
      UTF8ToUTF16(gpu_info.pixel_shader_version).c_str(),
      UTF8ToUTF16(gpu_info.vertex_shader_version).c_str());
}

void SetNumberOfViews(int number_of_views) {
  static MainSetNumberOfViews set_number_of_views = NULL;
  if (!set_number_of_views) {
    HMODULE exe_module = GetModuleHandle(chrome::kBrowserProcessExecutableName);
    if (!exe_module)
      return;
    set_number_of_views = reinterpret_cast<MainSetNumberOfViews>(
        GetProcAddress(exe_module, "SetNumberOfViews"));
    if (!set_number_of_views)
      return;
  }
  (set_number_of_views)(number_of_views);
}

}  // namespace child_process_logging
