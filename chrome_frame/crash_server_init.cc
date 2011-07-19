// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/crash_server_init.h"

#include <sddl.h>
#include <Shlobj.h>
#include <stdlib.h>
#include "version.h"  // NOLINT

const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

extern "C" IMAGE_DOS_HEADER __ImageBase;

// Builds a string representation of the user's SID and places it in user_sid.
bool GetUserSidString(std::wstring* user_sid) {
  bool success = false;
  if (user_sid) {
    struct {
      TOKEN_USER token_user;
      BYTE buffer[SECURITY_MAX_SID_SIZE];
    } token_info_buffer;

    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
      DWORD out_size;
      if (GetTokenInformation(token, TokenUser, &token_info_buffer.token_user,
                              sizeof(token_info_buffer), &out_size)) {
        wchar_t* user_sid_value = NULL;
        if (token_info_buffer.token_user.User.Sid &&
            ConvertSidToStringSid(token_info_buffer.token_user.User.Sid,
                                  &user_sid_value)) {
          *user_sid = user_sid_value;
          LocalFree(user_sid_value);
          user_sid_value = NULL;
          success = true;
        }
      }
      CloseHandle(token);
    }
  }

  return success;
}

bool IsRunningSystemInstall() {
  wchar_t exe_path[MAX_PATH * 2] = {0};
  GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase),
                    exe_path,
                    _countof(exe_path));

  bool is_system = false;

  wchar_t program_files_path[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                SHGFP_TYPE_CURRENT, program_files_path))) {
    if (wcsstr(exe_path, program_files_path) == exe_path) {
      is_system = true;
    }
  }

  return is_system;
}

google_breakpad::CustomClientInfo* GetCustomInfo() {
  static google_breakpad::CustomInfoEntry ver_entry(
      L"ver", TEXT(CHROME_VERSION_STRING));
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", L"ChromeFrame");
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", L"chrome_frame");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, ARRAYSIZE(entries) };
  return &custom_info;
}

google_breakpad::ExceptionHandler* InitializeCrashReporting(
    CrashReportingMode mode) {
  wchar_t temp_path[MAX_PATH + 1] = {0};
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);

  std::wstring pipe_name;
  if (mode == HEADLESS) {
    // This flag is used for testing, connect to the test crash service.
    pipe_name = kChromePipeName;
  } else {
    // Otherwise, build a pipe name corresponding to either user or
    // system-level Omaha.
    pipe_name = kGoogleUpdatePipeName;
    if (IsRunningSystemInstall()) {
      pipe_name += kSystemPrincipalSid;
    } else {
      std::wstring user_sid;
      if (GetUserSidString(&user_sid)) {
        pipe_name += user_sid;
      } else {
        // We don't think we're a system install, but we couldn't get the
        // user SID. Try connecting to the system-level crash service as a
        // last ditch effort.
        pipe_name += kSystemPrincipalSid;
      }
    }
  }

  google_breakpad::ExceptionHandler* breakpad =
      new google_breakpad::ExceptionHandler(
          temp_path, NULL, NULL, NULL,
          google_breakpad::ExceptionHandler::HANDLER_ALL, kLargerDumpType,
          pipe_name.c_str(), GetCustomInfo());

  return breakpad;
}
