// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <shellapi.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
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
