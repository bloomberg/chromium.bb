// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <winternl.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

// Statics

FilePath ScopedChromeFrameRegistrar::GetChromeFrameBuildPath() {
  FilePath build_path;
  PathService::Get(chrome::DIR_APP, &build_path);
  build_path = build_path.Append(L"servers").
                          Append(L"npchrome_tab.dll");
  file_util::PathExists(build_path);
  return build_path;
}

void ScopedChromeFrameRegistrar::RegisterDefaults() {
  FilePath dll_path = GetChromeFrameBuildPath();
  RegisterAtPath(dll_path.value());
}

void ScopedChromeFrameRegistrar::RegisterAtPath(
    const std::wstring& path) {

  ASSERT_FALSE(path.empty());
  HMODULE chrome_frame_dll_handle = LoadLibrary(path.c_str());
  ASSERT_TRUE(chrome_frame_dll_handle != NULL);

  typedef HRESULT (STDAPICALLTYPE* DllRegisterServerFn)();
  DllRegisterServerFn register_server =
      reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
          chrome_frame_dll_handle, "DllRegisterServer"));

  ASSERT_TRUE(register_server != NULL);
  EXPECT_HRESULT_SUCCEEDED((*register_server)());

  DllRegisterServerFn register_npapi_server =
      reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
          chrome_frame_dll_handle, "RegisterNPAPIPlugin"));

  if (register_npapi_server != NULL)
    EXPECT_HRESULT_SUCCEEDED((*register_npapi_server)());

  ASSERT_TRUE(FreeLibrary(chrome_frame_dll_handle));
}

// Non-statics

ScopedChromeFrameRegistrar::ScopedChromeFrameRegistrar() {
  original_dll_path_ = GetChromeFrameBuildPath().ToWStringHack();
  RegisterChromeFrameAtPath(original_dll_path_);
}

ScopedChromeFrameRegistrar::~ScopedChromeFrameRegistrar() {
  if (FilePath(original_dll_path_) != FilePath(new_chrome_frame_dll_path_)) {
    RegisterChromeFrameAtPath(original_dll_path_);
  }
}

void ScopedChromeFrameRegistrar::RegisterChromeFrameAtPath(
    const std::wstring& path) {
  RegisterAtPath(path);
  new_chrome_frame_dll_path_ = path;
}

void ScopedChromeFrameRegistrar::RegisterReferenceChromeFrameBuild() {
  std::wstring reference_build_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &reference_build_dir));

  file_util::UpOneDirectory(&reference_build_dir);
  file_util::UpOneDirectory(&reference_build_dir);

  file_util::AppendToPath(&reference_build_dir, L"chrome");
  file_util::AppendToPath(&reference_build_dir, L"tools");
  file_util::AppendToPath(&reference_build_dir, L"test");
  file_util::AppendToPath(&reference_build_dir, L"reference_build");
  file_util::AppendToPath(&reference_build_dir, L"chrome_frame");
  file_util::AppendToPath(&reference_build_dir, L"servers");
  file_util::AppendToPath(&reference_build_dir, L"npchrome_tab.dll");

  RegisterChromeFrameAtPath(reference_build_dir);
}

std::wstring ScopedChromeFrameRegistrar::GetChromeFrameDllPath() const {
  return new_chrome_frame_dll_path_;
}

// TODO(robertshield): The following could be factored out into its own file.
namespace {

typedef LONG WINAPI
NtQueryInformationProcess(
  IN HANDLE ProcessHandle,
  IN PROCESSINFOCLASS ProcessInformationClass,
  OUT PVOID ProcessInformation,
  IN ULONG ProcessInformationLength,
  OUT PULONG ReturnLength OPTIONAL
);

// Get the function pointer to NtQueryInformationProcess in NTDLL.DLL
static bool GetQIP(NtQueryInformationProcess** qip_func_ptr) {
  static NtQueryInformationProcess* qip_func =
      reinterpret_cast<NtQueryInformationProcess*>(
          GetProcAddress(GetModuleHandle(L"ntdll.dll"),
          "NtQueryInformationProcess"));
  DCHECK(qip_func) << "Could not get pointer to NtQueryInformationProcess.";
  *qip_func_ptr = qip_func;
  return qip_func != NULL;
}

// Get the command line of a process
bool GetCommandLineForProcess(uint32 process_id, std::wstring* cmd_line) {
  DCHECK(process_id != 0);
  DCHECK(cmd_line);

  // Open the process
  ScopedHandle process_handle(::OpenProcess(
                              PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                              false,
                              process_id));
  if (!process_handle) {
    DLOG(ERROR) << "Failed to open process " << process_id << ", last error = "
                << GetLastError();
  }

  // Obtain Process Environment Block
  NtQueryInformationProcess* qip_func = NULL;
  if (process_handle) {
    GetQIP(&qip_func);
  }

  // Read the address of the process params from the peb.
  DWORD process_params_address = 0;
  if (qip_func) {
    PROCESS_BASIC_INFORMATION info = { 0 };
    // NtQueryInformationProcess returns an NTSTATUS for whom negative values
    // are negative. Just check for that instead of pulling in DDK macros.
    if ((qip_func(process_handle.Get(),
                  ProcessBasicInformation,
                  &info,
                  sizeof(info),
                  NULL)) < 0) {
      DLOG(ERROR) << "Failed to invoke NtQueryProcessInformation, last error = "
                  << GetLastError();
    } else {
      BYTE* peb = reinterpret_cast<BYTE*>(info.PebBaseAddress);

      // The process command line parameters are (or were once) located at
      // the base address of the PEB + 0x10 for 32 bit processes. 64 bit
      // processes have a different PEB struct as per
      // http://msdn.microsoft.com/en-us/library/aa813706(VS.85).aspx.
      // TODO(robertshield): See about doing something about this.
      SIZE_T bytes_read = 0;
      if (!::ReadProcessMemory(process_handle.Get(),
                               peb + 0x10,
                               &process_params_address,
                               sizeof(process_params_address),
                               &bytes_read)) {
        DLOG(ERROR) << "Failed to read process params address, last error = "
                    << GetLastError();
      }
    }
  }

  // Copy all the process parameters into a buffer.
  bool success = false;
  std::wstring buffer;
  if (process_params_address) {
    SIZE_T bytes_read;
    RTL_USER_PROCESS_PARAMETERS params = { 0 };
    if (!::ReadProcessMemory(process_handle.Get(),
                             reinterpret_cast<void*>(process_params_address),
                             &params,
                             sizeof(params),
                             &bytes_read)) {
      DLOG(ERROR) << "Failed to read RTL_USER_PROCESS_PARAMETERS, "
                  << "last error = " << GetLastError();
    } else {
      // Read the command line parameter
      const int max_cmd_line_len = std::min(
          static_cast<int>(params.CommandLine.MaximumLength),
          4096);
      buffer.resize(max_cmd_line_len + 1);
      if (!::ReadProcessMemory(process_handle.Get(),
                               params.CommandLine.Buffer,
                               &buffer[0],
                               max_cmd_line_len,
                               &bytes_read)) {
        DLOG(ERROR) << "Failed to copy process command line, "
                    << "last error = " << GetLastError();
      } else {
        *cmd_line = buffer;
        success = true;
      }
    }
  }

  return success;
}

// Used to filter processes by process ID.
class ArgumentFilter : public base::ProcessFilter {
 public:
  explicit ArgumentFilter(const std::wstring& argument)
      : argument_to_find_(argument) {}

  // Returns true to indicate set-inclusion and false otherwise.  This method
  // should not have side-effects and should be idempotent.
  virtual bool Includes(base::ProcessId pid, base::ProcessId parent_pid) const {
    bool found = false;
    std::wstring command_line;
    if (GetCommandLineForProcess(pid, &command_line)) {
      std::wstring::const_iterator it =
          std::search(command_line.begin(),
                      command_line.end(),
                      argument_to_find_.begin(),
                      argument_to_find_.end(),
          CaseInsensitiveCompareASCII<wchar_t>());
      found = (it != command_line.end());
    }
    return found;
  }

 protected:
  std::wstring argument_to_find_;
};

}  // namespace

bool KillAllNamedProcessesWithArgument(const std::wstring& process_name,
                                       const std::wstring& argument) {
  bool result = true;
  const ProcessEntry* entry;
  ArgumentFilter filter(argument);
  base::NamedProcessIterator iter(process_name, &filter);
  while (entry = iter.NextProcessEntry()) {
    if (!base::KillProcessById((*entry).th32ProcessID, 0, true)) {
      DLOG(ERROR) << "Failed to kill process " << (*entry).th32ProcessID;
      result = false;
    }
  }

  return result;
}
