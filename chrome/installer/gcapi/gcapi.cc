// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This code is a legacy utility API for partners to check whether
//       Chrome can be installed and launched. Recent updates are being made
//       to add new functionality. These updates use code from Chromium, the old
//       coded against the win32 api directly. If you have an itch to shave a
//       yak, feel free to re-write the old code too.

#include "chrome/installer/gcapi/gcapi.h"

#include <sddl.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <tlhelp32.h>
#include <windows.h>

#include <cstdlib>
#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

#include "google_update_idl.h"  // NOLINT

using base::Time;
using base::TimeDelta;
using base::win::RegKey;
using base::win::ScopedCOMInitializer;
using base::win::ScopedComPtr;
using base::win::ScopedHandle;

namespace {

const wchar_t kChromeRegClientsKey[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegClientStateMediumKey[] =
    L"Software\\Google\\Update\\ClientStateMedium\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

const wchar_t kChromeRegLaunchCmd[] = L"InstallerSuccessLaunchCmdLine";
const wchar_t kChromeRegLastLaunchCmd[] = L"LastInstallerSuccessLaunchCmdLine";
const wchar_t kChromeRegVersion[] = L"pv";
const wchar_t kNoChromeOfferUntil[] =
    L"SOFTWARE\\Google\\No Chrome Offer Until";

const wchar_t kChromeWindowClass[] = L"Chrome_WidgetWin_0";

// Return the company name specified in the file version info resource.
bool GetCompanyName(const wchar_t* filename, wchar_t* buffer, DWORD out_len) {
  wchar_t file_version_info[8192];
  DWORD handle = 0;
  DWORD buffer_size = 0;

  buffer_size = ::GetFileVersionInfoSize(filename, &handle);
  // Cannot stats the file or our buffer size is too small (very unlikely).
  if (buffer_size == 0 || buffer_size > _countof(file_version_info))
    return false;

  buffer_size = _countof(file_version_info);
  memset(file_version_info, 0, buffer_size);
  if (!::GetFileVersionInfo(filename, handle, buffer_size, file_version_info))
    return false;

  DWORD data_len = 0;
  LPVOID data = NULL;
  // Retrieve the language and codepage code if exists.
  buffer_size = 0;
  if (!::VerQueryValue(file_version_info, TEXT("\\VarFileInfo\\Translation"),
      reinterpret_cast<LPVOID *>(&data), reinterpret_cast<UINT *>(&data_len)))
    return false;
  if (data_len != 4)
    return false;

  wchar_t info_name[256];
  DWORD lang = 0;
  // Formulate the string to retrieve the company name of the specific
  // language codepage.
  memcpy(&lang, data, 4);
  ::StringCchPrintf(info_name, _countof(info_name),
      L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName",
      (lang & 0xff00)>>8, (lang & 0xff), (lang & 0xff000000)>>24,
      (lang & 0xff0000)>>16);

  data_len = 0;
  if (!::VerQueryValue(file_version_info, info_name,
      reinterpret_cast<LPVOID *>(&data), reinterpret_cast<UINT *>(&data_len)))
    return false;
  if (data_len <= 0 || data_len >= (out_len / sizeof(wchar_t)))
    return false;

  memset(buffer, 0, out_len);
  ::StringCchCopyN(buffer,
                   (out_len / sizeof(wchar_t)),
                   reinterpret_cast<const wchar_t*>(data),
                   data_len);
  return true;
}

// Return true if we can re-offer Chrome; false, otherwise.
// Each partner can only offer Chrome once every six months.
bool CanReOfferChrome(BOOL set_flag) {
  wchar_t filename[MAX_PATH+1];
  wchar_t company[MAX_PATH];

  // If we cannot retrieve the version info of the executable or company
  // name, we allow the Chrome to be offered because there is no past
  // history to be found.
  if (::GetModuleFileName(NULL, filename, MAX_PATH) == 0)
    return true;
  if (!GetCompanyName(filename, company, sizeof(company)))
    return true;

  bool can_re_offer = true;
  DWORD disposition = 0;
  HKEY key = NULL;
  if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, kNoChromeOfferUntil,
      0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
      NULL, &key, &disposition) == ERROR_SUCCESS) {
    // Get today's date, and format it as YYYYMMDD numeric value.
    SYSTEMTIME now;
    GetLocalTime(&now);
    DWORD today = now.wYear * 10000 + now.wMonth * 100 + now.wDay;

    // Cannot re-offer, if the timer already exists and is not expired yet.
    DWORD value_type = REG_DWORD;
    DWORD value_data = 0;
    DWORD value_length = sizeof(DWORD);
    if (::RegQueryValueEx(key, company, 0, &value_type,
                          reinterpret_cast<LPBYTE>(&value_data),
                          &value_length) == ERROR_SUCCESS &&
        REG_DWORD == value_type &&
        value_data > today) {
      // The time has not expired, we cannot offer Chrome.
      can_re_offer = false;
    } else {
      // Delete the old or invalid value.
      ::RegDeleteValue(key, company);
      if (set_flag) {
        // Set expiration date for offer as six months from today,
        // represented as a YYYYMMDD numeric value.
        SYSTEMTIME timer = now;
        timer.wMonth = timer.wMonth + 6;
        if (timer.wMonth > 12) {
          timer.wMonth = timer.wMonth - 12;
          timer.wYear = timer.wYear + 1;
        }
        DWORD value = timer.wYear * 10000 + timer.wMonth * 100 + timer.wDay;
        ::RegSetValueEx(key, company, 0, REG_DWORD, (LPBYTE)&value,
                        sizeof(DWORD));
      }
    }

    ::RegCloseKey(key);
  }

  return can_re_offer;
}

// Helper function to read a value from registry. Returns true if value
// is read successfully and stored in parameter value. Returns false otherwise.
bool ReadValueFromRegistry(HKEY root_key, const wchar_t* sub_key,
                           const wchar_t* value_name, wchar_t* value,
                           size_t* size) {
  HKEY key;
  if ((::RegOpenKeyEx(root_key, sub_key, NULL,
                      KEY_READ, &key) == ERROR_SUCCESS) &&
      (::RegQueryValueEx(key, value_name, NULL, NULL,
                         reinterpret_cast<LPBYTE>(value),
                         reinterpret_cast<LPDWORD>(size)) == ERROR_SUCCESS)) {
    ::RegCloseKey(key);
    return true;
  }
  return false;
}

bool IsChromeInstalled(HKEY root_key) {
  wchar_t version[64];
  size_t size = _countof(version);
  return ReadValueFromRegistry(root_key, kChromeRegClientsKey,
                               kChromeRegVersion, version, &size);
}

enum WindowsVersion {
  VERSION_BELOW_XP_SP2,
  VERSION_XP_SP2_UP_TO_VISTA,  // "but not including"
  VERSION_VISTA_OR_HIGHER,
};
WindowsVersion GetWindowsVersion() {
  OSVERSIONINFOEX version_info = { sizeof version_info };
  GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));

  // Windows Vista is version 6.0.
  if (version_info.dwMajorVersion >= 6)
    return VERSION_VISTA_OR_HIGHER;

  // Windows XP is version 5.1.  (5.2 is Windows Server 2003/XP Pro x64.)
  if ((version_info.dwMajorVersion < 5) || (version_info.dwMinorVersion < 1))
    return VERSION_BELOW_XP_SP2;

  // For XP itself, we only support SP2 and above.
  return ((version_info.dwMinorVersion > 1) ||
          (version_info.wServicePackMajor >= 2)) ?
      VERSION_XP_SP2_UP_TO_VISTA : VERSION_BELOW_XP_SP2;
}

// Note this function should not be called on old Windows versions where these
// Windows API are not available. We always invoke this function after checking
// that current OS is Vista or later.
bool VerifyAdminGroup() {
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  PSID Group;
  BOOL check = ::AllocateAndInitializeSid(&NtAuthority, 2,
                                          SECURITY_BUILTIN_DOMAIN_RID,
                                          DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0,
                                          0, 0, 0,
                                          &Group);
  if (check) {
    if (!::CheckTokenMembership(NULL, Group, &check))
      check = FALSE;
  }
  ::FreeSid(Group);
  return (check == TRUE);
}

bool VerifyHKLMAccess(const wchar_t* sub_key) {
  HKEY root = HKEY_LOCAL_MACHINE;
  wchar_t str[] = L"test";
  bool result = false;
  DWORD disposition = 0;
  HKEY key = NULL;

  if (::RegCreateKeyEx(root, sub_key, 0, NULL,
                       REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL,
                       &key, &disposition) == ERROR_SUCCESS) {
    if (::RegSetValueEx(key, str, 0, REG_SZ, (LPBYTE)str,
        (DWORD)lstrlen(str)) == ERROR_SUCCESS) {
      result = true;
      RegDeleteValue(key, str);
    }

    //  If we create the main key, delete the entire key.
    if (disposition == REG_CREATED_NEW_KEY)
      RegDeleteKey(key, NULL);

    RegCloseKey(key);
  }

  return result;
}

bool IsRunningElevated() {
  // This method should be called only for Vista or later.
  if ((GetWindowsVersion() < VERSION_VISTA_OR_HIGHER) ||
      !VerifyAdminGroup())
    return false;

  HANDLE process_token;
  if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    return false;

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(process_token, TokenElevationType,
                             &elevation_type, sizeof(elevation_type),
                             &size_returned)) {
    ::CloseHandle(process_token);
    return false;
  }

  ::CloseHandle(process_token);
  return (elevation_type == TokenElevationTypeFull);
}

bool GetUserIdForProcess(size_t pid, wchar_t** user_sid) {
  HANDLE process_handle = ::OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, pid);
  if (process_handle == NULL)
    return false;

  HANDLE process_token;
  bool result = false;
  if (::OpenProcessToken(process_handle, TOKEN_QUERY, &process_token)) {
    DWORD size = 0;
    ::GetTokenInformation(process_token, TokenUser, NULL, 0, &size);
    if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER ||
        ::GetLastError() == ERROR_SUCCESS) {
      DWORD actual_size = 0;
      BYTE* token_user = new BYTE[size];
      if ((::GetTokenInformation(process_token, TokenUser, token_user, size,
                                &actual_size)) &&
          (actual_size <= size)) {
        PSID sid = reinterpret_cast<TOKEN_USER*>(token_user)->User.Sid;
        if (::ConvertSidToStringSid(sid, user_sid))
          result = true;
      }
      delete[] token_user;
    }
    ::CloseHandle(process_token);
  }
  ::CloseHandle(process_handle);
  return result;
}
}  // namespace

BOOL __stdcall GoogleChromeCompatibilityCheck(BOOL set_flag, DWORD* reasons) {
  DWORD local_reasons = 0;

  WindowsVersion windows_version = GetWindowsVersion();
  // System requirements?
  if (windows_version == VERSION_BELOW_XP_SP2)
    local_reasons |= GCCC_ERROR_OSNOTSUPPORTED;

  if (IsChromeInstalled(HKEY_LOCAL_MACHINE))
    local_reasons |= GCCC_ERROR_SYSTEMLEVELALREADYPRESENT;

  if (IsChromeInstalled(HKEY_CURRENT_USER))
    local_reasons |= GCCC_ERROR_USERLEVELALREADYPRESENT;

  if (!VerifyHKLMAccess(kChromeRegClientsKey)) {
    local_reasons |= GCCC_ERROR_ACCESSDENIED;
  } else if ((windows_version == VERSION_VISTA_OR_HIGHER) &&
             !VerifyAdminGroup()) {
    // For Vista or later check for elevation since even for admin user we could
    // be running in non-elevated mode. We require integrity level High.
    local_reasons |= GCCC_ERROR_INTEGRITYLEVEL;
  }

  // Then only check whether we can re-offer, if everything else is OK.
  if (local_reasons == 0 && !CanReOfferChrome(set_flag))
    local_reasons |= GCCC_ERROR_ALREADYOFFERED;

  // Done. Copy/return results.
  if (reasons != NULL)
    *reasons = local_reasons;

  return (local_reasons == 0);
}

BOOL __stdcall LaunchGoogleChrome() {
  // Check to make sure we have a valid Chrome installation.
  HKEY install_key = HKEY_LOCAL_MACHINE;
  if (!IsChromeInstalled(install_key)) {
    install_key = HKEY_CURRENT_USER;
    if (!IsChromeInstalled(install_key)) {
      return false;
    }
  }

  // Now grab the uninstall string from the appropriate ClientState key
  // and use that as the base for a path to chrome.exe.
  FilePath chrome_exe_path;
  RegKey client_state(install_key, kChromeRegClientStateKey, KEY_QUERY_VALUE);
  if (client_state.Valid()) {
    std::wstring uninstall_string;
    if (client_state.ReadValue(installer::kUninstallStringField,
                               &uninstall_string) == ERROR_SUCCESS) {
      // The uninstall path contains the path to setup.exe which is two levels
      // down from chrome.exe. Move up two levels (plus one to drop the file
      // name) and look for chrome.exe from there.
      FilePath uninstall_path(uninstall_string);
      chrome_exe_path = uninstall_path.DirName()
                                      .DirName()
                                      .DirName()
                                      .Append(installer::kChromeExe);
      if (!file_util::PathExists(chrome_exe_path)) {
        // By way of mild future proofing, look up one to see if there's a
        // chrome.exe in the version directory
        chrome_exe_path =
            uninstall_path.DirName().DirName().Append(installer::kChromeExe);
      }
    }
  }

  if (!file_util::PathExists(chrome_exe_path)) {
    return false;
  }

  ScopedCOMInitializer com_initializer;
  if (::CoInitializeSecurity(NULL, -1, NULL, NULL,
                             RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                             RPC_C_IMP_LEVEL_IDENTIFY, NULL,
                             EOAC_DYNAMIC_CLOAKING, NULL) != S_OK) {
    return false;
  }

  bool impersonation_success = false;
  if (IsRunningElevated()) {
    wchar_t* curr_proc_sid;
    if (!GetUserIdForProcess(GetCurrentProcessId(), &curr_proc_sid)) {
      return false;
    }

    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetShellWindow(), &pid);
    if (pid <= 0) {
      ::LocalFree(curr_proc_sid);
      return false;
    }

    wchar_t* exp_proc_sid;
    if (GetUserIdForProcess(pid, &exp_proc_sid)) {
      if (_wcsicmp(curr_proc_sid, exp_proc_sid) == 0) {
        ScopedHandle process_handle(
            ::OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION,
                          TRUE,
                          pid));
        if (process_handle.IsValid()) {
          HANDLE process_token = NULL;
          HANDLE user_token = NULL;
          if (::OpenProcessToken(process_handle, TOKEN_DUPLICATE | TOKEN_QUERY,
                                 &process_token) &&
              ::DuplicateTokenEx(process_token,
                                 TOKEN_IMPERSONATE | TOKEN_QUERY |
                                     TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE,
                                 NULL, SecurityImpersonation,
                                 TokenPrimary, &user_token) &&
              (::ImpersonateLoggedOnUser(user_token) != 0)) {
            impersonation_success = true;
          }
          if (user_token)
            ::CloseHandle(user_token);
          if (process_token)
            ::CloseHandle(process_token);
        }
      }
      ::LocalFree(exp_proc_sid);
    }

    ::LocalFree(curr_proc_sid);
    if (!impersonation_success) {
      return false;
    }
  }

  bool ret = false;
  ScopedComPtr<IProcessLauncher> ipl;
  if (SUCCEEDED(ipl.CreateInstance(__uuidof(ProcessLauncherClass),
                                   NULL,
                                   CLSCTX_LOCAL_SERVER))) {
    if (SUCCEEDED(ipl->LaunchCmdLine(chrome_exe_path.value().c_str())))
      ret = true;
    ipl.Release();
  }

  if (impersonation_success)
    ::RevertToSelf();
  return ret;
}

BOOL __stdcall LaunchGoogleChromeWithDimensions(int x,
                                                int y,
                                                int width,
                                                int height,
                                                bool in_background) {
  if (!LaunchGoogleChrome())
    return false;

  HWND handle = NULL;
  int seconds_elapsed = 0;

  // Chrome may have been launched, but the window may not have appeared
  // yet. Wait for it to appear for 10 seconds, but exit if it takes longer
  // than that.
  while (!handle && seconds_elapsed < 10) {
    handle = FindWindowEx(NULL, handle, kChromeWindowClass, NULL);
    if (!handle) {
      Sleep(1000);
      seconds_elapsed++;
    }
  }

  if (!handle)
    return false;

  // At this point, there are several top-level Chrome windows
  // but we only want the window that has child windows.

  // This loop iterates through all of the top-level Windows named
  // kChromeWindowClass, and looks for the first one with any children.
  while (handle && !FindWindowEx(handle, NULL, kChromeWindowClass, NULL)) {
    // Get the next top-level Chrome window.
    handle = FindWindowEx(NULL, handle, kChromeWindowClass, NULL);
  }

  HWND set_window_hwnd_insert_after = in_background ? HWND_BOTTOM : NULL;
  DWORD set_window_flags = in_background ? SWP_NOACTIVATE : SWP_NOZORDER;

  return (handle &&
          SetWindowPos(handle, set_window_hwnd_insert_after, x, y,
                       width, height, set_window_flags));
}

int __stdcall GoogleChromeDaysSinceLastRun() {
  int days_since_last_run = std::numeric_limits<int>::max();

  struct {
    HKEY hive;
    const wchar_t* path;
  } reg_data[] = {
    { HKEY_LOCAL_MACHINE, kChromeRegClientStateMediumKey },
    { HKEY_CURRENT_USER, kChromeRegClientStateKey }
  };

  for (int i = 0; i < arraysize(reg_data); ++i) {
    if (IsChromeInstalled(reg_data[i].hive)) {
      RegKey client_state(reg_data[i].hive, reg_data[i].path, KEY_QUERY_VALUE);
      if (client_state.Valid()) {
        std::wstring last_run;
        int64 last_run_value = 0;
        if (client_state.ReadValue(google_update::kRegLastRunTimeField,
                                   &last_run) == ERROR_SUCCESS &&
            base::StringToInt64(last_run, &last_run_value)) {
          Time last_run_time = Time::FromInternalValue(last_run_value);
          TimeDelta difference = Time::NowFromSystemTime() - last_run_time;

          // We can end up with negative numbers here, given changes in system
          // clock time or due to TimeDelta's int64 -> int truncation.
          int new_days_since_last_run = difference.InDays();
          if (new_days_since_last_run >= 0 &&
              new_days_since_last_run < days_since_last_run) {
            days_since_last_run = new_days_since_last_run;
          }
        }
      }
    }
  }

  if (days_since_last_run == std::numeric_limits<int>::max()) {
    days_since_last_run = -1;
  }

  return days_since_last_run;
}
