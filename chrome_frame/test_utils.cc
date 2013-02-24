// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <shellapi.h>
#include <winternl.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

const wchar_t kChromeFrameDllName[] = L"npchrome_frame.dll";
const wchar_t kChromeLauncherExeName[] = L"chrome_launcher.exe";
// How long to wait for DLLs to register or unregister themselves before killing
// the registrar.
const int64 kDllRegistrationTimeoutMs = 30 * 1000;

base::FilePath GetChromeFrameBuildPath() {
  base::FilePath build_path;
  PathService::Get(chrome::DIR_APP, &build_path);

  base::FilePath dll_path = build_path.Append(kChromeFrameDllName);

  if (!file_util::PathExists(dll_path)) {
    // Well, dang.. try looking in the current directory.
    dll_path = build_path.Append(kChromeFrameDllName);
  }

  if (!file_util::PathExists(dll_path)) {
    // No luck, return something empty.
    dll_path = base::FilePath();
  }

  return dll_path;
}

const wchar_t ScopedChromeFrameRegistrar::kCallRegistrationEntrypointSwitch[] =
    L"--call-registration-entrypoint";

bool ScopedChromeFrameRegistrar::register_chrome_path_provider_ = false;

// static
void ScopedChromeFrameRegistrar::RegisterDefaults() {
  if (!register_chrome_path_provider_) {
    chrome::RegisterPathProvider();
    register_chrome_path_provider_ = true;
  }
}

// Registers or unregisters the DLL at |path| by calling out to the current
// executable with --call-registration-entrypoint.  Loading the DLL into the
// test process is problematic for component=shared_library builds since
// singletons in base.dll aren't designed to handle multiple initialization.
// Use of rundll32.exe is problematic since it does not return useful error
// information.
// static
void ScopedChromeFrameRegistrar::DoRegistration(
    const string16& path,
    RegistrationType registration_type,
    RegistrationOperation registration_operation) {
  static const char* const kEntrypoints[] = {
    "DllRegisterServer",
    "DllUnregisterServer",
    "DllRegisterUserServer",
    "DllUnregisterUserServer",
  };

  DCHECK(!path.empty());
  DCHECK(registration_type == PER_USER || registration_type == SYSTEM_LEVEL);
  DCHECK(registration_operation == REGISTER ||
         registration_operation == UNREGISTER);

  int entrypoint_index = 0;
  base::LaunchOptions launch_options;
  base::ProcessHandle process_handle = INVALID_HANDLE_VALUE;
  int exit_code = -1;

  if (registration_type == PER_USER)
    entrypoint_index += 2;
  if (registration_operation == UNREGISTER)
    entrypoint_index += 1;
  string16 registration_command(ASCIIToUTF16("\""));
  registration_command +=
      CommandLine::ForCurrentProcess()->GetProgram().value();
  registration_command += ASCIIToUTF16("\" ");
  registration_command += kCallRegistrationEntrypointSwitch;
  registration_command += ASCIIToUTF16(" \"");
  registration_command += path;
  registration_command += ASCIIToUTF16("\" ");
  registration_command += ASCIIToUTF16(kEntrypoints[entrypoint_index]);
  launch_options.wait = true;
  if (!base::LaunchProcess(registration_command, launch_options,
                           &process_handle)) {
    PLOG(FATAL)
        << "Failed to register or unregister DLL with command: "
        << registration_command;
  } else {
    base::win::ScopedHandle rundll32(process_handle);
    if (!base::WaitForExitCodeWithTimeout(
            process_handle, &exit_code,
            base::TimeDelta::FromMilliseconds(kDllRegistrationTimeoutMs))) {
      LOG(ERROR) << "Timeout waiting to register or unregister DLL with "
                    "command: " << registration_command;
      base::KillProcess(process_handle, 0, false);
      NOTREACHED() << "Aborting test due to registration failure.";
    }
  }
  if (exit_code != 0) {
    if (registration_operation == REGISTER) {
      LOG(ERROR)
          << "DLL registration failed (exit code: 0x" << std::hex << exit_code
          << ", command: " << registration_command
          << "). Make sure you are running as Admin.";
      ::ExitProcess(1);
    } else {
      LOG(WARNING)
          << "DLL unregistration failed (exit code: 0x" << std::hex << exit_code
          << ", command: " << registration_command << ").";
    }
  }
}

// static
void ScopedChromeFrameRegistrar::RegisterAtPath(
    const std::wstring& path, RegistrationType registration_type) {
  DoRegistration(path, registration_type, REGISTER);
}

// static
void ScopedChromeFrameRegistrar::UnregisterAtPath(
    const std::wstring& path, RegistrationType registration_type) {
  DoRegistration(path, registration_type, UNREGISTER);
}

base::FilePath ScopedChromeFrameRegistrar::GetReferenceChromeFrameDllPath() {
  base::FilePath reference_build_dir;
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

// static
void ScopedChromeFrameRegistrar::RegisterAndExitProcessIfDirected() {
  // This method is invoked before any Chromium helpers have been initialized.
  // Take pains to use only Win32 and CRT functions.
  int argc = 0;
  const wchar_t* const* argv = ::CommandLineToArgvW(::GetCommandLine(), &argc);
  if (argc < 2 || ::lstrcmp(argv[1], kCallRegistrationEntrypointSwitch) != 0)
    return;
  if (argc != 4) {
    printf("Usage: %S %S <path to dll> <entrypoint>\n", argv[0],
           kCallRegistrationEntrypointSwitch);
    return;
  }

  // The only way to leave from here on down is ExitProcess.
  const wchar_t* dll_path = argv[2];
  const wchar_t* wide_entrypoint = argv[3];
  char entrypoint[256];
  HRESULT exit_code = 0;
  int entrypoint_len = lstrlen(wide_entrypoint);
  if (entrypoint_len <= 0 || entrypoint_len >= arraysize(entrypoint)) {
    exit_code = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  } else {
    // Convert wide to narrow. Since the entrypoint must be a narrow string
    // anyway, it is safe to truncate each character like this.
    std::copy(wide_entrypoint, wide_entrypoint + entrypoint_len + 1,
              &entrypoint[0]);
    HMODULE dll_module = ::LoadLibrary(dll_path);
    if (dll_module == NULL) {
      exit_code = HRESULT_FROM_WIN32(::GetLastError());
    } else {
      typedef HRESULT (STDAPICALLTYPE *RegisterFp)();
      RegisterFp register_func =
          reinterpret_cast<RegisterFp>(::GetProcAddress(dll_module,
                                                        entrypoint));
      if (register_func == NULL) {
        exit_code = HRESULT_FROM_WIN32(::GetLastError());
      } else {
        exit_code = register_func();
      }
      ::FreeLibrary(dll_module);
    }
  }

  ::ExitProcess(exit_code);
}

// Non-statics

ScopedChromeFrameRegistrar::ScopedChromeFrameRegistrar(
    const std::wstring& path, RegistrationType registration_type)
    : registration_type_(registration_type) {
  if (!register_chrome_path_provider_) {
    // Register paths needed by the ScopedChromeFrameRegistrar.
    chrome::RegisterPathProvider();
    register_chrome_path_provider_ = true;
  }
  original_dll_path_ = path;
  RegisterChromeFrameAtPath(original_dll_path_);
}

ScopedChromeFrameRegistrar::ScopedChromeFrameRegistrar(
    RegistrationType registration_type)
    : registration_type_(registration_type) {
  if (!register_chrome_path_provider_) {
    // Register paths needed by the ScopedChromeFrameRegistrar.
    chrome::RegisterPathProvider();
    register_chrome_path_provider_ = true;
  }
  original_dll_path_ = GetChromeFrameBuildPath().value();
  RegisterChromeFrameAtPath(original_dll_path_);
}

ScopedChromeFrameRegistrar::~ScopedChromeFrameRegistrar() {
  if (base::FilePath(original_dll_path_) !=
      base::FilePath(new_chrome_frame_dll_path_)) {
    RegisterChromeFrameAtPath(original_dll_path_);
  } else if (registration_type_ == PER_USER) {
    UnregisterAtPath(new_chrome_frame_dll_path_, registration_type_);
    HWND chrome_frame_helper_window =
        FindWindow(L"ChromeFrameHelperWindowClass", NULL);
    if (IsWindow(chrome_frame_helper_window)) {
      PostMessage(chrome_frame_helper_window, WM_CLOSE, 0, 0);
    } else {
      base::KillProcesses(L"chrome_frame_helper.exe", 0, NULL);
    }
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
  return base::KillProcesses(process_name, 0, &ArgumentFilter(argument));
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
