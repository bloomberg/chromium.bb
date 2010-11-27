// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions to interact with IE.

#include "ceee/ie/common/ie_util.h"

#include <atlcomcli.h>
#include <exdisp.h>  // IWebBrowser2

#include "base/logging.h"
#include "base/win/registry.h"
#include "base/string_util.h"
#include "ceee/common/com_utils.h"

namespace {

const wchar_t kIeVersionKey[] = L"SOFTWARE\\Microsoft\\Internet Explorer";
const wchar_t kIeVersionValue[] = L"Version";

HRESULT GetShellWindowsEnum(IEnumVARIANT** enum_windows) {
  CComPtr<IShellWindows> shell_windows;
  HRESULT hr = shell_windows.CoCreateInstance(CLSID_ShellWindows);
  DCHECK(SUCCEEDED(hr)) << "Could not CoCreate ShellWindows. " <<
      com::LogHr(hr);
  if (FAILED(hr))
    return hr;
  CComPtr<IUnknown> enum_punk;
  hr = shell_windows->_NewEnum(&enum_punk);
  DCHECK(SUCCEEDED(hr)) << "Could not get Enum IUnknown??? " <<
      com::LogHr(hr);
  if (FAILED(hr))
    return hr;
  return enum_punk->QueryInterface(IID_IEnumVARIANT,
                                   reinterpret_cast<void**>(enum_windows));
}

bool GetIeVersionString(std::wstring* version) {
  DCHECK(version != NULL);
  if (version == NULL)
    return false;
  base::win::RegKey key(HKEY_LOCAL_MACHINE, kIeVersionKey, KEY_READ);
  DCHECK(key.ValueExists(kIeVersionValue));
  return key.ReadValue(kIeVersionValue, version);
}

}  // namespace

namespace ie_util {

HRESULT GetWebBrowserForTopLevelIeHwnd(
    HWND window, IWebBrowser2* not_him, IWebBrowser2** browser) {
  DCHECK(browser != NULL);
  CComPtr<IEnumVARIANT> enum_windows;
  HRESULT hr = GetShellWindowsEnum(&enum_windows);
  DCHECK(SUCCEEDED(hr)) << "Could not get ShellWindows enum. " <<
      com::LogHr(hr);
  if (FAILED(hr))
    return hr;

  hr = enum_windows->Reset();
  DCHECK(SUCCEEDED(hr)) << "Can't Reset??? " << com::LogHr(hr);
  CComVariant shell_window;
  ULONG fetched = 0;
  while (SUCCEEDED(enum_windows->Next(1, &shell_window, &fetched)) &&
         fetched == 1) {
    DCHECK(shell_window.vt == VT_DISPATCH);
    CComQIPtr<IWebBrowser2> this_browser(shell_window.pdispVal);
    if (this_browser != NULL) {
      HWND this_window = NULL;
      hr = this_browser->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&this_window));
      // This can happen if the browser gets deconnected as we loop.
      if (SUCCEEDED(hr) && this_window == window && not_him != this_browser) {
        return this_browser.CopyTo(browser);
      }
    }
    shell_window.Clear();
  }
  return E_FAIL;
}

IeVersion GetIeVersion() {
  std::wstring ie_version_str;
  if (GetIeVersionString(&ie_version_str)) {
    int ie_version_num = wcstol(
        ie_version_str.substr(0, ie_version_str.find(L'.')).c_str(), NULL, 10);
    if (ie_version_num < 6)
      return IEVERSION_PRE_IE6;
    else if (ie_version_num == 6)
      return IEVERSION_IE6;
    else if (ie_version_num == 7)
      return IEVERSION_IE7;
    else if (ie_version_num == 8)
      return IEVERSION_IE8;
    else if (ie_version_num == 9)
      return IEVERSION_IE9;
  }
  DCHECK(false) << "Failed to get a known IE version!!!";
  return IEVERSION_UNKNOWN;
}

bool GetIEIsInPrivateBrowsing() {
  // TODO(skare@google.com): unify with version in chrome_frame/utils.cc

  // InPrivate flag will not change over process lifetime, so cache it. See:
  // http://blogs.msdn.com/ieinternals/archive/2009/06/30/IE8-Privacy-APIs-for-Addons.aspx
  static bool inprivate_status_cached = false;
  static bool is_inprivate = false;
  if (inprivate_status_cached)
    return is_inprivate;

  // InPrivate is only supported with IE8+.
  if (GetIeVersion() < IEVERSION_IE8) {
    inprivate_status_cached = true;
    return false;
  }

  typedef BOOL (WINAPI* IEIsInPrivateBrowsingPtr)();
  HMODULE ieframe_dll = GetModuleHandle(L"ieframe.dll");
  if (ieframe_dll) {
    IEIsInPrivateBrowsingPtr IsInPrivate =
        reinterpret_cast<IEIsInPrivateBrowsingPtr>(GetProcAddress(
        ieframe_dll, "IEIsInPrivateBrowsing"));
    if (IsInPrivate) {
      is_inprivate = !!IsInPrivate();
    }
  }

  inprivate_status_cached = true;
  return is_inprivate;
}

int GetAverageAddonLoadTimeMs(const CLSID& addon_id) {
  if (GetIeVersion() < IEVERSION_IE8)
    return kInvalidTime;

  std::wstring addon_id_str;
  if (!com::GuidToString(addon_id, &addon_id_str)) {
    NOTREACHED();
    return kInvalidTime;
  }

  std::wstring key_name =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Ext\\Stats\\";
  key_name += addon_id_str;
  key_name += L"\\iexplore";

  base::win::RegKey stats_key(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ);
  if (!stats_key.Valid()) {
    LOG(ERROR) << "Missing stats key: " << key_name;
    return kInvalidTime;
  }

  DWORD load_time = 0;
  if (GetIeVersion() < IEVERSION_IE9) {
    if (!stats_key.ReadValueDW(L"LoadTime", &load_time)) {
      LOG(ERROR) << "Can't read LoadTime.";
      return kInvalidTime;
    }
  } else {
    DWORD count = 0;
    int32 values[100];
    DWORD data_size = sizeof(values);
    DWORD data_type = REG_NONE;

    if (!stats_key.ReadValue(L"LoadTimeArray", &values, &data_size,
                             &data_type)) {
      LOG(ERROR) << "Can't read LoadTime.";
      return kInvalidTime;
    }

    if (data_type != REG_BINARY) {
      LOG(ERROR) << "Unexpected data type:" << data_type;
      return kInvalidTime;
    }

    if (data_size % sizeof(values[0]) != 0) {
      LOG(ERROR) << "Unexpected data length:" << data_size;
      return kInvalidTime;
    }

    data_size /= sizeof(values[0]);

    for (DWORD i = 0; i < data_size; ++i) {
      // Unused values are filled with negative values.
      if (values[i] >= 0) {
        load_time += values[i];
        ++count;
      }
    }

    if (count < 2) {
      // IE9 shows performance warning only after second run.
      LOG(INFO) << "Not enough data.";
      return kInvalidTime;
    }
    load_time /= count;
  }

  if (load_time < 0) {
    LOG(ERROR) << "Invalid time:" << load_time;
    return kInvalidTime;
  }

  LOG(INFO) << "Average add-on " << addon_id_str << "load time: " <<
      load_time << "ms";
  return load_time;
}

}  // namespace ie_util
