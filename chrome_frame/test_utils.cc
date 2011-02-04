// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome_frame/test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <winternl.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

const wchar_t kChromeFrameDllName[] = L"npchrome_frame.dll";
const wchar_t kChromeLauncherExeName[] = L"chrome_launcher.exe";

// Statics
FilePath ScopedChromeFrameRegistrar::GetChromeFrameBuildPath() {
  FilePath build_path;
  PathService::Get(chrome::DIR_APP, &build_path);

  FilePath dll_path = build_path.Append(L"servers").
                                 Append(kChromeFrameDllName);

  if (!file_util::PathExists(dll_path)) {
    // Well, dang.. try looking in the current directory.
    dll_path = build_path.Append(kChromeFrameDllName);
  }

  if (!file_util::PathExists(dll_path)) {
    // No luck, return something empty.
    dll_path = FilePath();
  }

  return dll_path;
}

void ScopedChromeFrameRegistrar::RegisterDefaults() {
  FilePath dll_path = GetChromeFrameBuildPath();
  RegisterAtPath(dll_path.value(), chrome_frame_test::GetTestBedType());
}

void ScopedChromeFrameRegistrar::RegisterAtPath(
    const std::wstring& path, RegistrationType registration_type) {

  ASSERT_FALSE(path.empty());
  HMODULE dll_handle = LoadLibrary(path.c_str());
  ASSERT_TRUE(dll_handle != NULL);

  typedef HRESULT (STDAPICALLTYPE* DllRegisterServerFn)();
  DllRegisterServerFn register_server = NULL;

  if (registration_type == PER_USER) {
      register_server = reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
          dll_handle, "DllRegisterUserServer"));
  } else {
      register_server = reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
          dll_handle, "DllRegisterServer"));
  }
  ASSERT_TRUE(register_server != NULL);
  EXPECT_HRESULT_SUCCEEDED((*register_server)());

  DllRegisterServerFn register_npapi_server = NULL;
  if (registration_type == PER_USER) {
    register_npapi_server =
        reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
            dll_handle, "RegisterNPAPIUserPlugin"));
  } else {
    register_npapi_server =
        reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
            dll_handle, "RegisterNPAPIPlugin"));
  }

  if (register_npapi_server != NULL)
    EXPECT_HRESULT_SUCCEEDED((*register_npapi_server)());

  ASSERT_TRUE(FreeLibrary(dll_handle));
}

void ScopedChromeFrameRegistrar::UnregisterAtPath(
    const std::wstring& path, RegistrationType registration_type) {

  ASSERT_FALSE(path.empty());
  HMODULE dll_handle = LoadLibrary(path.c_str());
  ASSERT_TRUE(dll_handle != NULL);

  typedef HRESULT (STDAPICALLTYPE* DllUnregisterServerFn)();
  DllUnregisterServerFn unregister_server = NULL;
  if (registration_type == PER_USER) {
    unregister_server = reinterpret_cast<DllUnregisterServerFn>
        (GetProcAddress(dll_handle, "DllUnregisterUserServer"));
  } else {
    unregister_server = reinterpret_cast<DllUnregisterServerFn>
        (GetProcAddress(dll_handle, "DllUnregisterServer"));
  }
  ASSERT_TRUE(unregister_server != NULL);
  EXPECT_HRESULT_SUCCEEDED((*unregister_server)());

  DllUnregisterServerFn unregister_npapi_server = NULL;
  if (registration_type == PER_USER) {
    unregister_npapi_server =
        reinterpret_cast<DllUnregisterServerFn>(GetProcAddress(
            dll_handle, "UnregisterNPAPIUserPlugin"));
  } else {
    unregister_npapi_server =
        reinterpret_cast<DllUnregisterServerFn>(GetProcAddress(
            dll_handle, "UnregisterNPAPIPlugin"));
  }

  if (unregister_npapi_server != NULL)
    EXPECT_HRESULT_SUCCEEDED((*unregister_npapi_server)());

  ASSERT_TRUE(FreeLibrary(dll_handle));
}

FilePath ScopedChromeFrameRegistrar::GetReferenceChromeFrameDllPath() {
  FilePath reference_build_dir;
  PathService::Get(chrome::DIR_APP, &reference_build_dir);

  reference_build_dir = reference_build_dir.DirName();
  reference_build_dir = reference_build_dir.DirName();

  reference_build_dir = reference_build_dir.AppendASCII("chrome_frame");
  reference_build_dir = reference_build_dir.AppendASCII("tools");
  reference_build_dir = reference_build_dir.AppendASCII("test");
  reference_build_dir = reference_build_dir.AppendASCII("reference_build");
  reference_build_dir = reference_build_dir.AppendASCII("chrome");
  reference_build_dir = reference_build_dir.AppendASCII("servers");
  reference_build_dir = reference_build_dir.Append(kChromeFrameDllName);
  return reference_build_dir;
}

// Non-statics

ScopedChromeFrameRegistrar::ScopedChromeFrameRegistrar(
    const std::wstring& path, RegistrationType registration_type)
    : registration_type_(registration_type) {
  original_dll_path_ = path;
  RegisterChromeFrameAtPath(original_dll_path_);
}

ScopedChromeFrameRegistrar::ScopedChromeFrameRegistrar(
    RegistrationType registration_type)
    : registration_type_(registration_type) {
  original_dll_path_ = GetChromeFrameBuildPath().value();
  RegisterChromeFrameAtPath(original_dll_path_);
}

ScopedChromeFrameRegistrar::~ScopedChromeFrameRegistrar() {
  if (FilePath(original_dll_path_) != FilePath(new_chrome_frame_dll_path_)) {
    RegisterChromeFrameAtPath(original_dll_path_);
  } else if (registration_type_ == PER_USER) {
    UnregisterAtPath(new_chrome_frame_dll_path_, registration_type_);
    chrome_frame_test::KillProcesses(L"chrome_frame_helper.exe", 0, false);
  }
}

void ScopedChromeFrameRegistrar::RegisterChromeFrameAtPath(
    const std::wstring& path) {
  RegisterAtPath(path, registration_type_);
  new_chrome_frame_dll_path_ = path;
}

void ScopedChromeFrameRegistrar::RegisterReferenceChromeFrameBuild() {
  RegisterChromeFrameAtPath(GetReferenceChromeFrameDllPath().value());
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
  base::win::ScopedHandle process_handle(::OpenProcess(
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
  virtual bool Includes(const base::ProcessEntry& entry) const {
    bool found = false;
    std::wstring command_line;
    if (GetCommandLineForProcess(entry.pid(), &command_line)) {
      std::wstring::const_iterator it =
          std::search(command_line.begin(),
                      command_line.end(),
                      argument_to_find_.begin(),
                      argument_to_find_.end(),
          base::CaseInsensitiveCompareASCII<wchar_t>());
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
  ArgumentFilter filter(argument);
  base::NamedProcessIterator iter(process_name, &filter);
  while (const base::ProcessEntry* entry = iter.NextProcessEntry()) {
    if (!base::KillProcessById(entry->pid(), 0, true)) {
      result = false;
    }
  }

  return result;
}

bool IsWorkstationLocked() {
  bool is_locked = true;
  HDESK input_desk = ::OpenInputDesktop(0, 0, GENERIC_READ);
  if (input_desk)  {
    wchar_t name[256] = {0};
    DWORD needed = 0;
    if (::GetUserObjectInformation(input_desk,
      UOI_NAME,
      name,
      sizeof(name),
      &needed)) {
        is_locked = lstrcmpi(name, L"default") != 0;
    }
    ::CloseDesktop(input_desk);
  }
  return is_locked;
}
