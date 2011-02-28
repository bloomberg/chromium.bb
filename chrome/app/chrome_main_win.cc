// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

#include <malloc.h>
#include <new.h>
#include <shlobj.h>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "policy/policy_constants.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#endif

namespace {

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
bool LoadUserDataDirPolicyFromRegistry(HKEY hive, FilePath* user_data_dir) {
  std::wstring key_name = UTF8ToWide(policy::key::kUserDataDir);
  std::wstring value;

  base::win::RegKey hklm_policy_key(hive, policy::kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *user_data_dir = FilePath::FromWStringHack(
        policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

}  // namespace

namespace chrome_main {

void LowLevelInit() {
  RegisterInvalidParamHandler();
}

void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  // Policy from the HKLM hive has precedence over HKCU so if we have one here
  // we don't have to try to load HKCU.
  if (!LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, user_data_dir))
    LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, user_data_dir);
}

}  // namespace chrome_main
