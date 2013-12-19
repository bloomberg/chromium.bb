// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/blacklist/blacklist.h"

#include <string.h>

#include "base/basictypes.h"
#include "chrome_elf/blacklist/blacklist_interceptions.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/service_resolver.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace blacklist{

const wchar_t* g_troublesome_dlls[kTroublesomeDllsMaxCount] = {};
int g_troublesome_dlls_cur_index = 0;

const wchar_t kRegistryBeaconPath[] = L"SOFTWARE\\Google\\Chrome\\BLBeacon";

}  // namespace blacklist

// Allocate storage for thunks in a RWX page of this module to save on doing
// an extra allocation at run time.
#if !defined(_WIN64)
// 64-bit images appear to not support writeable and executable pages.
// This would yield compile warning C4330.
// TODO(robertshield): Add 64 bit support.
#pragma section(".crthunk",read,write,execute)
__declspec(allocate(".crthunk")) sandbox::ThunkData g_thunk_storage;
#endif

namespace {

enum Version {
  VERSION_PRE_XP_SP2 = 0,  // Not supported.
  VERSION_XP_SP2,
  VERSION_SERVER_2003, // Also includes XP Pro x64 and Server 2003 R2.
  VERSION_VISTA,       // Also includes Windows Server 2008.
  VERSION_WIN7,        // Also includes Windows Server 2008 R2.
  VERSION_WIN8,        // Also includes Windows Server 2012.
  VERSION_WIN8_1,
  VERSION_WIN_LAST,    // Indicates error condition.
};

// Whether a process is running under WOW64 (the wrapper that allows 32-bit
// processes to run on 64-bit versions of Windows).  This will return
// WOW64_DISABLED for both "32-bit Chrome on 32-bit Windows" and "64-bit
// Chrome on 64-bit Windows".  WOW64_UNKNOWN means "an error occurred", e.g.
// the process does not have sufficient access rights to determine this.
enum WOW64Status {
  WOW64_DISABLED,
  WOW64_ENABLED,
  WOW64_UNKNOWN,
};

WOW64Status GetWOW64StatusForCurrentProcess() {
  typedef BOOL (WINAPI* IsWow64ProcessFunc)(HANDLE, PBOOL);
  IsWow64ProcessFunc is_wow64_process = reinterpret_cast<IsWow64ProcessFunc>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return WOW64_DISABLED;
  BOOL is_wow64 = FALSE;
  if (!(*is_wow64_process)(GetCurrentProcess(), &is_wow64))
    return WOW64_UNKNOWN;
  return is_wow64 ? WOW64_ENABLED : WOW64_DISABLED;
}

class OSInfo {
 public:
  struct VersionNumber {
    int major;
    int minor;
    int build;
  };

  struct ServicePack {
    int major;
    int minor;
  };

  OSInfo() {
    OSVERSIONINFOEX version_info = { sizeof(version_info) };
    GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));
    version_number_.major = version_info.dwMajorVersion;
    version_number_.minor = version_info.dwMinorVersion;
    version_number_.build = version_info.dwBuildNumber;
    if ((version_number_.major == 5) && (version_number_.minor > 0)) {
      // Treat XP Pro x64, Home Server, and Server 2003 R2 as Server 2003.
      version_ = (version_number_.minor == 1) ? VERSION_XP_SP2 :
                                                VERSION_SERVER_2003;
      if (version_ == VERSION_XP_SP2 && version_info.wServicePackMajor < 2)
        version_ = VERSION_PRE_XP_SP2;
    } else if (version_number_.major == 6) {
      switch (version_number_.minor) {
        case 0:
          // Treat Windows Server 2008 the same as Windows Vista.
          version_ = VERSION_VISTA;
          break;
        case 1:
          // Treat Windows Server 2008 R2 the same as Windows 7.
          version_ = VERSION_WIN7;
          break;
        case 2:
          // Treat Windows Server 2012 the same as Windows 8.
          version_ = VERSION_WIN8;
          break;
        default:
          version_ = VERSION_WIN8_1;
          break;
      }
    } else if (version_number_.major > 6) {
      version_ = VERSION_WIN_LAST;
    } else {
      version_ = VERSION_PRE_XP_SP2;
    }

    service_pack_.major = version_info.wServicePackMajor;
    service_pack_.minor = version_info.wServicePackMinor;
  }

  Version version() const { return version_; }
  VersionNumber version_number() const { return version_number_; }
  ServicePack service_pack() const { return service_pack_; }

 private:
  Version version_;
  VersionNumber version_number_;
  ServicePack service_pack_;

  DISALLOW_COPY_AND_ASSIGN(OSInfo);
};

bool IsNonBrowserProcess() {
  wchar_t* command_line = GetCommandLine();
  return (command_line && wcsstr(command_line, L"--type"));
}

}  // namespace

namespace blacklist {

bool CreateBeacon() {
  HKEY beacon_key = NULL;
  DWORD disposition = 0;
  LONG result = ::RegCreateKeyEx(HKEY_CURRENT_USER,
                                 kRegistryBeaconPath,
                                 0,
                                 NULL,
                                 0,
                                 KEY_WRITE,
                                 NULL,
                                 &beacon_key,
                                 &disposition);
  bool success = (result == ERROR_SUCCESS &&
                  disposition != REG_OPENED_EXISTING_KEY);
  if (result == ERROR_SUCCESS)
    ::RegCloseKey(beacon_key);
  return success;
}

bool ClearBeacon() {
  LONG result = ::RegDeleteKey(HKEY_CURRENT_USER, kRegistryBeaconPath);
  return (result == ERROR_SUCCESS);
}

bool AddDllToBlacklist(const wchar_t* dll_name) {
  if (g_troublesome_dlls_cur_index >= kTroublesomeDllsMaxCount)
    return false;
  for (int i = 0; i < g_troublesome_dlls_cur_index; ++i) {
    if (!wcscmp(g_troublesome_dlls[i], dll_name))
      return true;
  }

  // Copy string to blacklist.
  wchar_t* str_buffer = new wchar_t[wcslen(dll_name) + 1];
  wcscpy(str_buffer, dll_name);

  g_troublesome_dlls[g_troublesome_dlls_cur_index] = str_buffer;
  g_troublesome_dlls_cur_index++;
  return true;
}

bool RemoveDllFromBlacklist(const wchar_t* dll_name) {
  for (int i = 0; i < g_troublesome_dlls_cur_index; ++i) {
    if (!wcscmp(g_troublesome_dlls[i], dll_name)) {
      // Found the thing to remove. Delete it then replace it with the last
      // element.
      g_troublesome_dlls_cur_index--;
      delete[] g_troublesome_dlls[i];
      g_troublesome_dlls[i] = g_troublesome_dlls[g_troublesome_dlls_cur_index];
      g_troublesome_dlls[g_troublesome_dlls_cur_index] = NULL;
      return true;
    }
  }
  return false;
}

bool Initialize(bool force) {
#if defined(_WIN64)
  // TODO(robertshield): Implement 64-bit support by providing 64-bit
  //                     interceptors.
  return false;
#endif

  // Check to see that we found the functions we need in ntdll.
  if (!InitializeInterceptImports())
    return false;

  // Check to see if this is a non-browser process, abort if so.
  if (IsNonBrowserProcess())
    return false;

  // Check to see if a beacon is present, abort if so.
  if (!force && !CreateBeacon())
    return false;

  // Don't try blacklisting on unsupported OS versions.
  OSInfo os_info;
  if (os_info.version() <= VERSION_PRE_XP_SP2)
    return false;

  // Pseudo-handle, no need to close.
  HANDLE current_process = ::GetCurrentProcess();

  // Tells the resolver to patch already patched functions.
  const bool kRelaxed = true;

  // Create a thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk* thunk;
#if defined(_WIN64)
  // TODO(robertshield): Use the appropriate thunk for 64-bit support
  // when said support is implemented.
#else
  if (GetWOW64StatusForCurrentProcess() == WOW64_ENABLED) {
    if (os_info.version() >= VERSION_WIN8)
      thunk = new sandbox::Wow64W8ResolverThunk(current_process, kRelaxed);
    else
      thunk = new sandbox::Wow64ResolverThunk(current_process, kRelaxed);
  } else if (os_info.version() >= VERSION_WIN8) {
    thunk = new sandbox::Win8ResolverThunk(current_process, kRelaxed);
  } else {
    thunk = new sandbox::ServiceResolverThunk(current_process, kRelaxed);
  }
#endif

#if defined(_WIN64)
  BYTE* thunk_storage = new BYTE[sizeof(sandbox::ThunkData)];
#else
  BYTE* thunk_storage = reinterpret_cast<BYTE*>(&g_thunk_storage);
#endif

  thunk->AllowLocalPatches();

  // Get ntdll base, target name, interceptor address,
  NTSTATUS ret = thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                              reinterpret_cast<void*>(&__ImageBase),
                              "NtMapViewOfSection",
                              NULL,
                              &blacklist::BlNtMapViewOfSection,
                              thunk_storage,
                              sizeof(sandbox::ThunkData),
                              NULL);

  delete thunk;
  return NT_SUCCESS(ret);
}

}  // namespace blacklist
