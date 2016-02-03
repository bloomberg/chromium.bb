// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <stddef.h>

#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/startup_metric_utils/common/pre_read_field_trial_utils_win.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simulate_crash_win.h"

namespace crash_reporter {
namespace internal {

namespace {

base::LazyInstance<crashpad::CrashpadClient>::Leaky g_crashpad_client =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void GetPlatformCrashpadAnnotations(
    std::map<std::string, std::string>* annotations) {
  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  base::FilePath exe_file;
  CHECK(PathService::Get(base::FILE_EXE, &exe_file));
  base::string16 product_name, version, special_build, channel_name;
  crash_reporter_client->GetProductNameAndVersion(
      exe_file, &product_name, &version, &special_build, &channel_name);
  (*annotations)["prod"] = base::UTF16ToUTF8(product_name);
  (*annotations)["ver"] = base::UTF16ToUTF8(version);
  (*annotations)["channel"] = base::UTF16ToUTF8(channel_name);
  if (!special_build.empty())
    (*annotations)["special"] = base::UTF16ToUTF8(special_build);
#if defined(ARCH_CPU_X86)
  (*annotations)["plat"] = std::string("Win32");
#elif defined(ARCH_CPU_X86_64)
  (*annotations)["plat"] = std::string("Win64");
#endif
}

base::FilePath PlatformCrashpadInitialization(bool initial_client,
                                              bool browser_process) {
  base::FilePath database_path;  // Only valid in the browser process.
  bool result;

  const char kPipeNameVar[] = "CHROME_CRASHPAD_PIPE_NAME";
  scoped_ptr<base::Environment> env(base::Environment::Create());

  DCHECK_EQ(initial_client, browser_process);

  if (initial_client) {
    CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
    crash_reporter_client->GetCrashDumpLocation(&database_path);

    std::map<std::string, std::string> process_annotations;
    GetPlatformCrashpadAnnotations(&process_annotations);

#if defined(GOOGLE_CHROME_BUILD)
    std::string url = "https://clients2.google.com/cr/report";
#else
    std::string url;
#endif

    std::vector<std::string> arguments;

    // In test binaries, use crashpad_handler directly. Otherwise, we launch
    // chrome.exe with --type=crashpad-handler.
    base::FilePath exe_file;
    CHECK(PathService::Get(base::FILE_EXE, &exe_file));
    if (exe_file.BaseName().value() != FILE_PATH_LITERAL("chrome.exe")) {
      base::FilePath exe_dir = exe_file.DirName();
      exe_file = exe_dir.Append(FILE_PATH_LITERAL("crashpad_handler.exe"));
    } else {
      arguments.push_back("--type=crashpad-handler");

      if (startup_metric_utils::GetPreReadOptions().use_prefetch_argument) {
        // The prefetch argument added here has to be documented in
        // chrome_switches.cc, below the kPrefetchArgument* constants. A
        // constant can't be used here because crashpad can't depend on Chrome.
        arguments.push_back("/prefetch:7");
      }
    }

    // TODO(scottmg): See https://crashpad.chromium.org/bug/23.
    arguments.push_back("--no-rate-limit");

    result = g_crashpad_client.Get().StartHandler(
        exe_file, database_path, url, process_annotations, arguments, false);

    // If we're the browser, push the pipe name into the environment so child
    // processes can connect to it. If we inherited another crashpad_handler's
    // pipe name, we'll overwrite it here.
    env->SetVar(kPipeNameVar,
                base::UTF16ToUTF8(g_crashpad_client.Get().GetHandlerIPCPipe()));
  } else {
    std::string pipe_name_utf8;
    result = env->GetVar(kPipeNameVar, &pipe_name_utf8);
    if (result) {
      result = g_crashpad_client.Get().SetHandlerIPCPipe(
          base::UTF8ToUTF16(pipe_name_utf8));
    }
  }

  if (result) {
    result = g_crashpad_client.Get().UseHandler();
  }

  return database_path;
}

// TODO(scottmg): http://crbug.com/546288 These exported functions are for
// compatibility with how Breakpad worked, but it seems like there's no need to
// do the CreateRemoteThread() dance with a minor extension of the Crashpad API
// (to just pass the pid we want a dump for). We should add that and then modify
// hang_crash_dump_win.cc to work in a more direct manner.

// Used for dumping a process state when there is no crash.
extern "C" void __declspec(dllexport) __cdecl DumpProcessWithoutCrash() {
  CRASHPAD_SIMULATE_CRASH();
}

namespace {

// We need to prevent ICF from folding DumpForHangDebuggingThread() and
// DumpProcessWithoutCrashThread() together, since that makes them
// indistinguishable in crash dumps. We do this by making the function
// bodies unique, and prevent optimization from shuffling things around.
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

// Note that this function must be in a namespace for the [Renderer hang]
// annotations to work on the crash server.
DWORD WINAPI DumpProcessWithoutCrashThread(void*) {
  DumpProcessWithoutCrash();
  return 0;
}

// The following two functions do exactly the same thing as the two above. But
// we want the signatures to be different so that we can easily track them in
// crash reports.
// TODO(yzshen): Remove when enough information is collected and the hang rate
// of pepper/renderer processes is reduced.
DWORD WINAPI DumpForHangDebuggingThread(void*) {
  DumpProcessWithoutCrash();
  VLOG(1) << "dumped for hang debugging";
  return 0;
}

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE()

}  // namespace

}  // namespace internal
}  // namespace crash_reporter

extern "C" {

// Crashes the process after generating a dump for the provided exception. Note
// that the crash reporter should be initialized before calling this function
// for it to do anything.
// NOTE: This function is used by SyzyASAN to invoke a crash. If you change the
// the name or signature of this function you will break SyzyASAN instrumented
// releases of Chrome. Please contact syzygy-team@chromium.org before doing so!
int __declspec(dllexport) CrashForException(
    EXCEPTION_POINTERS* info) {
  crash_reporter::internal::g_crashpad_client.Get().DumpAndCrash(info);
  return EXCEPTION_CONTINUE_SEARCH;
}

// Injects a thread into a remote process to dump state when there is no crash.
HANDLE __declspec(dllexport) __cdecl InjectDumpProcessWithoutCrash(
    HANDLE process) {
  return CreateRemoteThread(
      process, NULL, 0, crash_reporter::internal::DumpProcessWithoutCrashThread,
      0, 0, NULL);
}

HANDLE __declspec(dllexport) __cdecl InjectDumpForHangDebugging(
    HANDLE process) {
  return CreateRemoteThread(
      process, NULL, 0, crash_reporter::internal::DumpForHangDebuggingThread, 0,
      0, NULL);
}

#if defined(ARCH_CPU_X86_64)

static int CrashForExceptionInNonABICompliantCodeRange(
    PEXCEPTION_RECORD ExceptionRecord,
    ULONG64 EstablisherFrame,
    PCONTEXT ContextRecord,
    PDISPATCHER_CONTEXT DispatcherContext) {
  EXCEPTION_POINTERS info = { ExceptionRecord, ContextRecord };
  return CrashForException(&info);
}

// See https://msdn.microsoft.com/en-us/library/ddssxxy8.aspx
typedef struct _UNWIND_INFO {
  unsigned char Version : 3;
  unsigned char Flags : 5;
  unsigned char SizeOfProlog;
  unsigned char CountOfCodes;
  unsigned char FrameRegister : 4;
  unsigned char FrameOffset : 4;
  ULONG ExceptionHandler;
} UNWIND_INFO, *PUNWIND_INFO;

struct ExceptionHandlerRecord {
  RUNTIME_FUNCTION runtime_function;
  UNWIND_INFO unwind_info;
  unsigned char thunk[12];
};

// These are GetProcAddress()d from V8 binding code.
void __declspec(dllexport) __cdecl RegisterNonABICompliantCodeRange(
    void* start,
    size_t size_in_bytes) {
  ExceptionHandlerRecord* record =
      reinterpret_cast<ExceptionHandlerRecord*>(start);

  // We assume that the first page of the code range is executable and
  // committed and reserved for breakpad. What could possibly go wrong?

  // All addresses are 32bit relative offsets to start.
  record->runtime_function.BeginAddress = 0;
  record->runtime_function.EndAddress =
      base::checked_cast<DWORD>(size_in_bytes);
  record->runtime_function.UnwindData =
      offsetof(ExceptionHandlerRecord, unwind_info);

  // Create unwind info that only specifies an exception handler.
  record->unwind_info.Version = 1;
  record->unwind_info.Flags = UNW_FLAG_EHANDLER;
  record->unwind_info.SizeOfProlog = 0;
  record->unwind_info.CountOfCodes = 0;
  record->unwind_info.FrameRegister = 0;
  record->unwind_info.FrameOffset = 0;
  record->unwind_info.ExceptionHandler =
      offsetof(ExceptionHandlerRecord, thunk);

  // Hardcoded thunk.
  // mov imm64, rax
  record->thunk[0] = 0x48;
  record->thunk[1] = 0xb8;
  void* handler = &CrashForExceptionInNonABICompliantCodeRange;
  memcpy(&record->thunk[2], &handler, 8);

  // jmp rax
  record->thunk[10] = 0xff;
  record->thunk[11] = 0xe0;

  // Protect reserved page against modifications.
  DWORD old_protect;
  CHECK(VirtualProtect(
      start, sizeof(ExceptionHandlerRecord), PAGE_EXECUTE_READ, &old_protect));
  CHECK(RtlAddFunctionTable(
      &record->runtime_function, 1, reinterpret_cast<DWORD64>(start)));
}

void __declspec(dllexport) __cdecl UnregisterNonABICompliantCodeRange(
    void* start) {
  ExceptionHandlerRecord* record =
      reinterpret_cast<ExceptionHandlerRecord*>(start);

  CHECK(RtlDeleteFunctionTable(&record->runtime_function));
}
#endif  // ARCH_CPU_X86_64

}  // extern "C"
