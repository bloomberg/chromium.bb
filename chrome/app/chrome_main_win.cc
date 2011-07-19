// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

#include <atlbase.h>
#include <atlapp.h>
#include <malloc.h>
#include <new.h>
#include <shlobj.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_switches.h"
#include "policy/policy_constants.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#endif

namespace {

CAppModule _Module;

#pragma optimize("", off)
// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
void InvalidParameter(const wchar_t* expression, const wchar_t* function,
                      const wchar_t* file, unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
  _exit(1);
}

void PureCall() {
  __debugbreak();
  _exit(1);
}

#pragma warning(push)
// Disables warning 4748 which is: "/GS can not protect parameters and local
// variables from local buffer overrun because optimizations are disabled in
// function."  GetStats() will not overflow the passed-in buffer and this
// function never returns.
#pragma warning(disable : 4748)
void OnNoMemory() {
#if defined(USE_TCMALLOC)
  // Try to get some information on the stack to make the crash easier to
  // diagnose from a minidump, being very careful not to do anything that might
  // try to heap allocate.
  char buf[32*1024];
  MallocExtension::instance()->GetStats(buf, sizeof(buf));
#endif
  // Kill the process. This is important for security, since WebKit doesn't
  // NULL-check many memory allocations. If a malloc fails, returns NULL, and
  // the buffer is then used, it provides a handy mapping of memory starting at
  // address 0 for an attacker to utilize.
  __debugbreak();
  _exit(1);
}
#pragma warning(pop)
#pragma optimize("", on)

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler() {
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  // Gather allocation failure.
  std::set_new_handler(&OnNoMemory);
  // Also enable the new handler for malloc() based failures.
  _set_new_mode(1);
}

// Checks if the registry key exists in the given hive and expands any
// variables in the string.
bool LoadUserDataDirPolicyFromRegistry(HKEY hive,
                                       const std::wstring& key_name,
                                       FilePath* user_data_dir) {
  std::wstring value;

  base::win::RegKey hklm_policy_key(hive, policy::kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *user_data_dir = FilePath(policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

}  // namespace

namespace chrome_main {

void LowLevelInit(void* instance) {
  RegisterInvalidParamHandler();

  _Module.Init(NULL, static_cast<HINSTANCE>(instance));
}

void LowLevelShutdown() {
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC

  _Module.Term();
}

void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  DCHECK(user_data_dir);
  // We are running as Chrome Frame if we were invoked with user-data-dir,
  // chrome-frame, and automation-channel switches.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const bool is_chrome_frame =
      !user_data_dir->empty() &&
      command_line->HasSwitch(switches::kChromeFrame) &&
      command_line->HasSwitch(switches::kAutomationClientChannelID);

  // In the case of Chrome Frame, the last path component of the user-data-dir
  // provided on the command line must be preserved since it is specific to
  // CF's host.
  FilePath cf_host_dir;
  if (is_chrome_frame)
    cf_host_dir = user_data_dir->BaseName();

  // Policy from the HKLM hive has precedence over HKCU so if we have one here
  // we don't have to try to load HKCU.
  const char* key_name_ascii = (is_chrome_frame ? policy::key::kGCFUserDataDir :
                                policy::key::kUserDataDir);
  std::wstring key_name(ASCIIToWide(key_name_ascii));
  if (LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, key_name,
                                        user_data_dir) ||
      LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, key_name,
                                        user_data_dir)) {
    // A Group Policy value was loaded.  Append the Chrome Frame host directory
    // if relevant.
    if (is_chrome_frame)
      *user_data_dir = user_data_dir->Append(cf_host_dir);
  }
}

}  // namespace chrome_main
