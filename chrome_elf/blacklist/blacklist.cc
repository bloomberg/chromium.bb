// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/blacklist/blacklist.h"

#include <assert.h>
#include <string.h>

#include "base/basictypes.h"
#include "chrome_elf/blacklist/blacklist_interceptions.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/chrome_elf_util.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/service_resolver.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace blacklist{

const wchar_t* g_troublesome_dlls[kTroublesomeDllsMaxCount] = {
  L"datamngr.dll",                      // Unknown (suspected adware).
  L"hk.dll",                            // Unknown (keystroke logger).
  L"libsvn_tsvn32.dll",                 // TortoiseSVN.
  L"lmrn.dll",                          // Unknown.
  // Keep this null pointer here to mark the end of the list.
  NULL,
};

bool g_blocked_dlls[kTroublesomeDllsMaxCount] = {};
int g_num_blocked_dlls = 0;

}  // namespace blacklist

// Allocate storage for thunks in a page of this module to save on doing
// an extra allocation at run time.
#pragma section(".crthunk",read,execute)
__declspec(allocate(".crthunk")) sandbox::ThunkData g_thunk_storage;

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

// Record if the blacklist was successfully initialized so processes can easily
// determine if the blacklist is enabled for them.
bool g_blacklist_initialized = false;

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

// Record that the thunk setup completed succesfully and close the registry
// key handle since it is no longer needed.
void RecordSuccessfulThunkSetup(HKEY* key) {
  if (key != NULL) {
    DWORD blacklist_state = blacklist::BLACKLIST_SETUP_RUNNING;
    ::RegSetValueEx(*key,
                    blacklist::kBeaconState,
                    0,
                    REG_DWORD,
                    reinterpret_cast<LPBYTE>(&blacklist_state),
                    sizeof(blacklist_state));
    ::RegCloseKey(*key);
    key = NULL;
  }
}

}  // namespace

namespace blacklist {

#if defined(_WIN64)
  // Allocate storage for the pointer to the old NtMapViewOfSectionFunction.
#pragma section(".oldntmap",write,read)
  __declspec(allocate(".oldntmap"))
    NtMapViewOfSectionFunction g_nt_map_view_of_section_func = NULL;
#endif

bool LeaveSetupBeacon() {
  HKEY key = NULL;
  DWORD disposition = 0;
  LONG result = ::RegCreateKeyEx(HKEY_CURRENT_USER,
                                 kRegistryBeaconPath,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE,
                                 NULL,
                                 &key,
                                 &disposition);
  if (result != ERROR_SUCCESS)
    return false;

  // Retrieve the current blacklist state.
  DWORD blacklist_state = BLACKLIST_DISABLED;
  DWORD blacklist_state_size = sizeof(blacklist_state);
  DWORD type = 0;
  result = ::RegQueryValueEx(key,
                             kBeaconState,
                             0,
                             &type,
                             reinterpret_cast<LPBYTE>(&blacklist_state),
                             &blacklist_state_size);

  if (blacklist_state != BLACKLIST_ENABLED ||
      result != ERROR_SUCCESS || type != REG_DWORD) {
    ::RegCloseKey(key);
    return false;
  }

  // Mark the blacklist setup code as running so if it crashes the blacklist
  // won't be enabled for the next run.
  blacklist_state = BLACKLIST_SETUP_RUNNING;
  result = ::RegSetValueEx(key,
                           kBeaconState,
                           0,
                           REG_DWORD,
                           reinterpret_cast<LPBYTE>(&blacklist_state),
                           sizeof(blacklist_state));
  ::RegCloseKey(key);

  return (result == ERROR_SUCCESS);
}

bool ResetBeacon() {
  HKEY key = NULL;
  DWORD disposition = 0;
  LONG result = ::RegCreateKeyEx(HKEY_CURRENT_USER,
                                 kRegistryBeaconPath,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE,
                                 NULL,
                                 &key,
                                 &disposition);
  if (result != ERROR_SUCCESS)
    return false;

  DWORD blacklist_state = BLACKLIST_ENABLED;
  result = ::RegSetValueEx(key,
                           kBeaconState,
                           0,
                           REG_DWORD,
                           reinterpret_cast<LPBYTE>(&blacklist_state),
                           sizeof(blacklist_state));
  ::RegCloseKey(key);

  return (result == ERROR_SUCCESS);
}

int BlacklistSize() {
  int size = -1;
  while (blacklist::g_troublesome_dlls[++size] != NULL) {}

  return size;
}

bool IsBlacklistInitialized() {
  return g_blacklist_initialized;
}

bool AddDllToBlacklist(const wchar_t* dll_name) {
  int blacklist_size = BlacklistSize();
  // We need to leave one space at the end for the null pointer.
  if (blacklist_size + 1 >= kTroublesomeDllsMaxCount)
    return false;
  for (int i = 0; i < blacklist_size; ++i) {
    if (!_wcsicmp(g_troublesome_dlls[i], dll_name))
      return true;
  }

  // Copy string to blacklist.
  wchar_t* str_buffer = new wchar_t[wcslen(dll_name) + 1];
  wcscpy(str_buffer, dll_name);

  g_troublesome_dlls[blacklist_size] = str_buffer;
  g_blocked_dlls[blacklist_size] = false;
  return true;
}

bool RemoveDllFromBlacklist(const wchar_t* dll_name) {
  int blacklist_size = BlacklistSize();
  for (int i = 0; i < blacklist_size; ++i) {
    if (!_wcsicmp(g_troublesome_dlls[i], dll_name)) {
      // Found the thing to remove. Delete it then replace it with the last
      // element.
      delete[] g_troublesome_dlls[i];
      g_troublesome_dlls[i] = g_troublesome_dlls[blacklist_size - 1];
      g_troublesome_dlls[blacklist_size - 1] = NULL;

      // Also update the stats recording if we have blocked this dll or not.
      if (g_blocked_dlls[i])
        --g_num_blocked_dlls;
      g_blocked_dlls[i] = g_blocked_dlls[blacklist_size - 1];
      return true;
    }
  }
  return false;
}

// TODO(csharp): Maybe store these values in the registry so we can
// still report them if Chrome crashes early.
void SuccessfullyBlocked(const wchar_t** blocked_dlls, int* size) {
  if (size == NULL)
    return;

  // If the array isn't valid or big enough, just report the size it needs to
  // be and return.
  if (blocked_dlls == NULL && *size < g_num_blocked_dlls) {
    *size = g_num_blocked_dlls;
    return;
  }

  *size = g_num_blocked_dlls;

  int strings_to_fill = 0;
  for (int i = 0; strings_to_fill < g_num_blocked_dlls && g_troublesome_dlls[i];
       ++i) {
    if (g_blocked_dlls[i]) {
      blocked_dlls[strings_to_fill] = g_troublesome_dlls[i];
      ++strings_to_fill;
    }
  }
}

void BlockedDll(size_t blocked_index) {
  assert(blocked_index < kTroublesomeDllsMaxCount);

  if (!g_blocked_dlls[blocked_index] &&
      blocked_index < kTroublesomeDllsMaxCount) {
    ++g_num_blocked_dlls;
    g_blocked_dlls[blocked_index] = true;
  }
}

bool Initialize(bool force) {
  // Check to see that we found the functions we need in ntdll.
  if (!InitializeInterceptImports())
    return false;

  // Check to see if this is a non-browser process, abort if so.
  if (IsNonBrowserProcess())
    return false;

  // Check to see if a beacon is present, abort if so.
  if (!force && !LeaveSetupBeacon())
    return false;

  // Don't try blacklisting on unsupported OS versions.
  OSInfo os_info;
  if (os_info.version() <= VERSION_PRE_XP_SP2)
    return false;

  // Pseudo-handle, no need to close.
  HANDLE current_process = ::GetCurrentProcess();

  // Tells the resolver to patch already patched functions.
  const bool kRelaxed = true;

  // Record that we are starting the thunk setup code.
  HKEY key = NULL;
  DWORD disposition = 0;
  LONG result = ::RegCreateKeyEx(HKEY_CURRENT_USER,
                                 kRegistryBeaconPath,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE,
                                 NULL,
                                 &key,
                                 &disposition);
  if (result == ERROR_SUCCESS) {
    DWORD blacklist_state = BLACKLIST_THUNK_SETUP;
    ::RegSetValueEx(key,
                    kBeaconState,
                    0,
                    REG_DWORD,
                    reinterpret_cast<LPBYTE>(&blacklist_state),
                    sizeof(blacklist_state));
  } else {
    key = NULL;
  }

  // Create a thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk* thunk = NULL;
#if defined(_WIN64)
  // Because Windows 8 and 8.1 have different stubs in 64-bit,
  // ServiceResolverThunk can handle all the formats in 64-bit (instead only
  // handling 1 like it does in 32-bit versions).
  thunk = new sandbox::ServiceResolverThunk(current_process, kRelaxed);
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

  // Record that we have initialized the blacklist.
  g_blacklist_initialized = true;

  BYTE* thunk_storage = reinterpret_cast<BYTE*>(&g_thunk_storage);

  // Mark the thunk storage as readable and writeable, since we
  // ready to write to it.
  DWORD old_protect = 0;
  if (!VirtualProtect(&g_thunk_storage,
                      sizeof(g_thunk_storage),
                      PAGE_EXECUTE_READWRITE,
                      &old_protect)) {
    RecordSuccessfulThunkSetup(&key);
    return false;
  }

  thunk->AllowLocalPatches();

  // We declare this early so it can be used in the 64-bit block below and
  // still work on 32-bit build when referenced at the end of the function.
  BOOL page_executable = false;

  // Replace the default NtMapViewOfSection with our patched version.
#if defined(_WIN64)
  NTSTATUS ret = thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                              reinterpret_cast<void*>(&__ImageBase),
                              "NtMapViewOfSection",
                              NULL,
                              &blacklist::BlNtMapViewOfSection64,
                              thunk_storage,
                              sizeof(sandbox::ThunkData),
                              NULL);

  // Keep a pointer to the original code, we don't have enough space to
  // add it directly to the call.
  g_nt_map_view_of_section_func = reinterpret_cast<NtMapViewOfSectionFunction>(
      thunk_storage);

  // Ensure that the pointer to the old function can't be changed.
 page_executable = VirtualProtect(&g_nt_map_view_of_section_func,
                                  sizeof(g_nt_map_view_of_section_func),
                                  PAGE_EXECUTE_READ,
                                  &old_protect);
#else
  NTSTATUS ret = thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                              reinterpret_cast<void*>(&__ImageBase),
                              "NtMapViewOfSection",
                              NULL,
                              &blacklist::BlNtMapViewOfSection,
                              thunk_storage,
                              sizeof(sandbox::ThunkData),
                              NULL);
#endif
  delete thunk;

  // Mark the thunk storage as executable and prevent any future writes to it.
  page_executable = page_executable && VirtualProtect(&g_thunk_storage,
                                                      sizeof(g_thunk_storage),
                                                      PAGE_EXECUTE_READ,
                                                      &old_protect);

  RecordSuccessfulThunkSetup(&key);

  return NT_SUCCESS(ret) && page_executable;
}

}  // namespace blacklist
