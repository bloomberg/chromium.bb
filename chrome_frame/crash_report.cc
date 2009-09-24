// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.cc : Implementation crash reporting.
#include "chrome_frame/crash_report.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome_frame/vectored_handler.h"
#include "chrome_frame/vectored_handler-impl.h"

namespace {
// TODO(joshia): factor out common code with chrome used for crash reporting
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";
// Well known SID for the system principal.
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";
google_breakpad::ExceptionHandler* g_breakpad = NULL;

// Returns the custom info structure based on the dll in parameter and the
// process type.
google_breakpad::CustomClientInfo* GetCustomInfo() {
  // TODO(joshia): Grab these based on build.
  static google_breakpad::CustomInfoEntry ver_entry(L"ver", L"0.1.0.0");
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", L"ChromeFrame");
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", L"chrome_frame");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, arraysize(entries) };
  return &custom_info;
}

__declspec(naked)
static EXCEPTION_REGISTRATION_RECORD* InternalRtlpGetExceptionList() {
  __asm {
    mov eax, fs:0
    ret
  }
}
}  // end of namespace

// Class which methods simply forwards to Win32 API and uses breakpad to write
// a minidump. Used as template (external interface) of VectoredHandlerT<E>.
class Win32VEHTraits : public VEHTraitsBase {
 public:
  static inline void* Register(PVECTORED_EXCEPTION_HANDLER func,
      const void* module_start, const void* module_end) {
    VEHTraitsBase::SetModule(module_start, module_end);
    return ::AddVectoredExceptionHandler(1, func);
  }

  static inline ULONG Unregister(void* handle) {
    return ::RemoveVectoredExceptionHandler(handle);
  }

  static inline bool WriteDump(EXCEPTION_POINTERS* p) {
    return g_breakpad->WriteMinidumpForException(p);
  }

  static inline EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
    return InternalRtlpGetExceptionList();
  }

  static inline WORD RtlCaptureStackBackTrace(DWORD FramesToSkip,
      DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash) {
    return ::RtlCaptureStackBackTrace(FramesToSkip, FramesToCapture,
                                      BackTrace, BackTraceHash);
  }
};

extern "C" IMAGE_DOS_HEADER __ImageBase;
bool InitializeCrashReporting(bool use_crash_service, bool full_dump) {
  if (g_breakpad)
    return true;

  std::wstring pipe_name;
  if (use_crash_service) {
    // Crash reporting is done by crash_service.exe.
    pipe_name = kChromePipeName;
  } else {
    // We want to use the Google Update crash reporting. We need to check if the
    // user allows it first.
    if (!GoogleUpdateSettings::GetCollectStatsConsent())
      return true;

    // Build the pipe name. It can be either:
    // System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
    // Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
    wchar_t dll_path[MAX_PATH * 2] = {0};
    GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase), dll_path, 
                      arraysize(dll_path));

    std::wstring user_sid;
    if (InstallUtil::IsPerUserInstall(dll_path)) {
      if (!win_util::GetUserSidString(&user_sid)) {
        return false;
      }
    } else {
      user_sid = kSystemPrincipalSid;
    }

    pipe_name = kGoogleUpdatePipeName;
    pipe_name += user_sid;
  }

  // Get the alternate dump directory. We use the temp path.
  FilePath temp_directory;
  if (!file_util::GetTempDir(&temp_directory) || temp_directory.empty()) {
    return false;
  }

  MINIDUMP_TYPE dump_type = full_dump ? MiniDumpWithFullMemory : MiniDumpNormal;
  g_breakpad = new google_breakpad::ExceptionHandler(
      temp_directory.value(), NULL, NULL, NULL,
      google_breakpad::ExceptionHandler::HANDLER_INVALID_PARAMETER |
      google_breakpad::ExceptionHandler::HANDLER_PURECALL, dump_type,
      pipe_name.c_str(), GetCustomInfo());

  if (g_breakpad) {
    // Find current module boundaries.
    const void* start = &__ImageBase;
    const char* s = reinterpret_cast<const char*>(start);
    const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>
        (s + __ImageBase.e_lfanew);
    const void* end = s + nt->OptionalHeader.SizeOfImage;
    VectoredHandler::Register(start, end);
  }

  return g_breakpad != NULL;
}

bool ShutdownCrashReporting() {
  VectoredHandler::Unregister();
  delete g_breakpad;
  g_breakpad = NULL;
  return true;
}
