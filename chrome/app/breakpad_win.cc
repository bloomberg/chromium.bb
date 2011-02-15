// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/breakpad_win.h"

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>

#include <algorithm>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/app/hard_error_handler_win.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "policy/policy_constants.h"

namespace {

// Minidump with stacks, PEB, TEB, and unloaded module list.
const MINIDUMP_TYPE kSmallDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

// Minidump with all of the above, plus memory referenced from stack.
const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

// Large dump with all process memory.
const MINIDUMP_TYPE kFullDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithFullMemory |  // Full memory from process.
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithHandleData |  // Get all handle information.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";

// This is the well known SID for the system principal.
const wchar_t kSystemPrincipalSid[] =L"S-1-5-18";

google_breakpad::ExceptionHandler* g_breakpad = NULL;

// A pointer to the custom entries that we send in the event of a crash. We need
// this pointer, along with the offsets into it below, so that we can keep the
// data updated as the state of the browser changes.
static std::vector<google_breakpad::CustomInfoEntry>* g_custom_entries = NULL;
static size_t g_url_chunks_offset;
static size_t g_num_of_extensions_offset;
static size_t g_extension_ids_offset;
static size_t g_client_id_offset;
static size_t g_gpu_info_offset;
static size_t g_num_of_views_offset;

// Dumps the current process memory.
extern "C" void __declspec(dllexport) __cdecl DumpProcess() {
  if (g_breakpad)
    g_breakpad->WriteMinidump();
}

// Reduces the size of the string |str| to a max of 64 chars. Required because
// breakpad's CustomInfoEntry raises an invalid_parameter error if the string
// we want to set is longer.
std::wstring TrimToBreakpadMax(const std::wstring& str) {
  std::wstring shorter(str);
  return shorter.substr(0,
      google_breakpad::CustomInfoEntry::kValueMaxLength - 1);
}

// Returns the custom info structure based on the dll in parameter and the
// process type.
google_breakpad::CustomClientInfo* GetCustomInfo(const std::wstring& dll_path,
                                                 const std::wstring& type) {
  scoped_ptr<FileVersionInfo>
      version_info(FileVersionInfo::CreateFileVersionInfo(FilePath(dll_path)));

  std::wstring version, product;
  if (version_info.get()) {
    // Get the information from the file.
    version = version_info->product_version();
    if (!version_info->is_official_build())
      version.append(L"-devel");

    const CommandLine& command = *CommandLine::ForCurrentProcess();
    if (command.HasSwitch(switches::kChromeFrame)) {
      product = L"ChromeFrame";
    } else {
      product = version_info->product_short_name();
    }
  } else {
    // No version info found. Make up the values.
     product = L"Chrome";
     version = L"0.0.0.0-devel";
  }

  // We only expect this method to be called once per process.
  DCHECK(!g_custom_entries);
  g_custom_entries = new std::vector<google_breakpad::CustomInfoEntry>;

  // Common g_custom_entries.
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"ver", version.c_str()));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"prod", product.c_str()));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"plat", L"Win32"));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"ptype", type.c_str()));

  g_num_of_extensions_offset = g_custom_entries->size();
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"num-extensions", L"N/A"));

  g_extension_ids_offset = g_custom_entries->size();
  for (int i = 0; i < kMaxReportedActiveExtensions; ++i) {
    g_custom_entries->push_back(google_breakpad::CustomInfoEntry(
        StringPrintf(L"extension-%i", i + 1).c_str(), L""));
  }

  // Add empty values for the gpu_info. We'll put the actual values
  // when we collect them at this location.
  g_gpu_info_offset = g_custom_entries->size();
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"gpu-venid", L""));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"gpu-devid", L""));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"gpu-driver", L""));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"gpu-psver", L""));
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"gpu-vsver", L""));

  // Read the id from registry. If reporting has never been enabled
  // the result will be empty string. Its OK since when user enables reporting
  // we will insert the new value at this location.
  std::wstring guid;
  GoogleUpdateSettings::GetMetricsId(&guid);
  g_client_id_offset = g_custom_entries->size();
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"guid", guid.c_str()));

  if (type == L"renderer" || type == L"plugin" || type == L"gpu-process") {
    g_num_of_views_offset = g_custom_entries->size();
    g_custom_entries->push_back(
        google_breakpad::CustomInfoEntry(L"num-views", L""));
    // Create entries for the URL. Currently we only allow each chunk to be 64
    // characters, which isn't enough for a URL. As a hack we create 8 entries
    // and split the URL across the g_custom_entries.
    g_url_chunks_offset = g_custom_entries->size();
    for (int i = 0; i < kMaxUrlChunks; ++i) {
      g_custom_entries->push_back(google_breakpad::CustomInfoEntry(
          StringPrintf(L"url-chunk-%i", i + 1).c_str(), L""));
    }
  } else {
    g_custom_entries->push_back(
        google_breakpad::CustomInfoEntry(L"num-views", L"N/A"));

    // Browser-specific g_custom_entries.
    google_breakpad::CustomInfoEntry switch1(L"switch-1", L"");
    google_breakpad::CustomInfoEntry switch2(L"switch-2", L"");

    // Get the first two command line switches if they exist. The CommandLine
    // class does not allow to enumerate the switches so we do it by hand.
    int num_args = 0;
    wchar_t** args = ::CommandLineToArgvW(::GetCommandLineW(), &num_args);
    if (args) {
      if (num_args > 1)
        switch1.set_value(TrimToBreakpadMax(args[1]).c_str());
      if (num_args > 2)
        switch2.set_value(TrimToBreakpadMax(args[2]).c_str());
      // The caller must free the memory allocated for |args|.
      ::LocalFree(args);
    }

    g_custom_entries->push_back(switch1);
    g_custom_entries->push_back(switch2);
  }

  static google_breakpad::CustomClientInfo custom_client_info;
  custom_client_info.entries = &g_custom_entries->front();
  custom_client_info.count = g_custom_entries->size();

  return &custom_client_info;
}

// Contains the information needed by the worker thread.
struct CrashReporterInfo {
  google_breakpad::CustomClientInfo* custom_info;
  std::wstring dll_path;
  std::wstring process_type;
};

// This callback is executed when the browser process has crashed, after
// the crash dump has been created. We need to minimize the amount of work
// done here since we have potentially corrupted process. Our job is to
// spawn another instance of chrome which will show a 'chrome has crashed'
// dialog. This code needs to live in the exe and thus has no access to
// facilities such as the i18n helpers.
bool DumpDoneCallback(const wchar_t*, const wchar_t*, void*,
                      EXCEPTION_POINTERS* ex_info,
                      MDRawAssertionInfo*, bool) {
  // If the exception is because there was a problem loading a delay-loaded
  // module, then show the user a dialog explaining the problem and then exit.
  if (DelayLoadFailureExceptionMessageBox(ex_info))
    return true;

  // We set CHROME_CRASHED env var. If the CHROME_RESTART is present.
  // This signals the child process to show the 'chrome has crashed' dialog.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(env_vars::kRestartInfo)) {
    return true;
  }
  env->SetVar(env_vars::kShowRestart, "1");
  // Now we just start chrome browser with the same command line.
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;
  if (::CreateProcessW(NULL, ::GetCommandLineW(), NULL, NULL, FALSE,
                       CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi)) {
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
  }
  // After this return we will be terminated. The actual return value is
  // not used at all.
  return true;
}

// flag to indicate that we are already handling an exception.
volatile LONG handling_exception = 0;

// This callback is executed when the Chrome process has crashed and *before*
// the crash dump is created. To prevent duplicate crash reports we
// make every thread calling this method, except the very first one,
// go to sleep.
bool FilterCallback(void*, EXCEPTION_POINTERS*, MDRawAssertionInfo*) {
  // Capture every thread except the first one in the sleep. We don't
  // want multiple threads to concurrently report exceptions.
  if (::InterlockedCompareExchange(&handling_exception, 1, 0) == 1) {
    ::Sleep(INFINITE);
  }
  return true;
}

// Previous unhandled filter. Will be called if not null when we
// intercept a crash.
LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = NULL;

// Exception filter used when breakpad is not enabled. We just display
// the "Do you want to restart" message and then we call the previous filter.
long WINAPI ChromeExceptionFilter(EXCEPTION_POINTERS* info) {
  DumpDoneCallback(NULL, NULL, NULL, info, NULL, false);

  if (previous_filter)
    return previous_filter(info);

  return EXCEPTION_EXECUTE_HANDLER;
}

// Exception filter for the service process used when breakpad is not enabled.
// We just display the "Do you want to restart" message and then die
// (without calling the previous filter).
long WINAPI ServiceExceptionFilter(EXCEPTION_POINTERS* info) {
  DumpDoneCallback(NULL, NULL, NULL, info, NULL, false);
  return EXCEPTION_EXECUTE_HANDLER;
}

extern "C" void __declspec(dllexport) __cdecl SetActiveURL(
    const wchar_t* url_cstring) {
  DCHECK(url_cstring);

  if (!g_custom_entries)
    return;

  std::wstring url(url_cstring);
  size_t chunk_index = 0;
  size_t url_size = url.size();

  // Split the url across all the chunks.
  for (size_t url_offset = 0;
       chunk_index < kMaxUrlChunks && url_offset < url_size; ++chunk_index) {
    size_t current_chunk_size = std::min(url_size - url_offset,
        static_cast<size_t>(
            google_breakpad::CustomInfoEntry::kValueMaxLength - 1));

    wchar_t* entry_value =
        (*g_custom_entries)[g_url_chunks_offset + chunk_index].value;
    url._Copy_s(entry_value,
                google_breakpad::CustomInfoEntry::kValueMaxLength,
                current_chunk_size, url_offset);
    entry_value[current_chunk_size] = L'\0';
    url_offset += current_chunk_size;
  }

  // And null terminate any unneeded chunks.
  for (; chunk_index < kMaxUrlChunks; ++chunk_index)
    (*g_custom_entries)[g_url_chunks_offset + chunk_index].value[0] = L'\0';
}

extern "C" void __declspec(dllexport) __cdecl SetClientId(
    const wchar_t* client_id) {
  if (client_id == NULL)
    return;

  if (!g_custom_entries)
    return;

  wcscpy_s((*g_custom_entries)[g_client_id_offset].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           client_id);
}

static void SetIntegerValue(size_t offset, int value) {
  if (!g_custom_entries)
    return;

  wcscpy_s((*g_custom_entries)[offset].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           StringPrintf(L"%d", value).c_str());
}

extern "C" void __declspec(dllexport) __cdecl SetNumberOfExtensions(
    int number_of_extensions) {
  SetIntegerValue(g_num_of_extensions_offset, number_of_extensions);
}

extern "C" void __declspec(dllexport) __cdecl SetExtensionID(
    int index, const wchar_t* id) {
  DCHECK(id);
  DCHECK(index < kMaxReportedActiveExtensions);

  if (!g_custom_entries)
    return;

  wcscpy_s((*g_custom_entries)[g_extension_ids_offset + index].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           id);
}

extern "C" void __declspec(dllexport) __cdecl SetGpuInfo(
    const wchar_t* vendor_id, const wchar_t* device_id,
    const wchar_t* driver_version, const wchar_t* pixel_shader_version,
    const wchar_t* vertex_shader_version) {
  if (!g_custom_entries)
    return;

  wcscpy_s((*g_custom_entries)[g_gpu_info_offset].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           vendor_id);
  wcscpy_s((*g_custom_entries)[g_gpu_info_offset+1].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           device_id);
  wcscpy_s((*g_custom_entries)[g_gpu_info_offset+2].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           driver_version);
  wcscpy_s((*g_custom_entries)[g_gpu_info_offset+3].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           pixel_shader_version);
  wcscpy_s((*g_custom_entries)[g_gpu_info_offset+4].value,
           google_breakpad::CustomInfoEntry::kValueMaxLength,
           vertex_shader_version);
}

extern "C" void __declspec(dllexport) __cdecl SetNumberOfViews(
    int number_of_views) {
  SetIntegerValue(g_num_of_views_offset, number_of_views);
}

}  // namespace

bool WrapMessageBoxWithSEH(const wchar_t* text, const wchar_t* caption,
                           UINT flags, bool* exit_now) {
  // We wrap the call to MessageBoxW with a SEH handler because it some
  // machines with CursorXP, PeaDict or with FontExplorer installed it crashes
  // uncontrollably here. Being this a best effort deal we better go away.
  __try {
    *exit_now = (IDOK != ::MessageBoxW(NULL, text, caption, flags));
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // Its not safe to continue executing, exit silently here.
    ::ExitProcess(ResultCodes::RESPAWN_FAILED);
  }

  return true;
}

// This function is executed by the child process that DumpDoneCallback()
// spawned and basically just shows the 'chrome has crashed' dialog if
// the CHROME_CRASHED environment variable is present.
bool ShowRestartDialogIfCrashed(bool* exit_now) {
  if (!::GetEnvironmentVariableW(ASCIIToWide(env_vars::kShowRestart).c_str(),
                                 NULL, 0)) {
    return false;
  }

  DWORD len = ::GetEnvironmentVariableW(
      ASCIIToWide(env_vars::kRestartInfo).c_str(), NULL, 0);
  if (!len)
    return true;

  wchar_t* restart_data = new wchar_t[len + 1];
  ::GetEnvironmentVariableW(ASCIIToWide(env_vars::kRestartInfo).c_str(),
                            restart_data, len);
  restart_data[len] = 0;
  // The CHROME_RESTART var contains the dialog strings separated by '|'.
  // See PrepareRestartOnCrashEnviroment() function for details.
  std::vector<std::wstring> dlg_strings;
  base::SplitString(restart_data, L'|', &dlg_strings);
  delete[] restart_data;
  if (dlg_strings.size() < 3)
    return true;

  // If the UI layout is right-to-left, we need to pass the appropriate MB_XXX
  // flags so that an RTL message box is displayed.
  UINT flags = MB_OKCANCEL | MB_ICONWARNING;
  if (dlg_strings[2] == ASCIIToWide(env_vars::kRtlLocale))
    flags |= MB_RIGHT | MB_RTLREADING;

  return WrapMessageBoxWithSEH(dlg_strings[1].c_str(), dlg_strings[0].c_str(),
                               flags, exit_now);
}

// Determine whether configuration management allows loading the crash reporter.
// Since the configuration management infrastructure is not initialized at this
// point, we read the corresponding registry key directly. The return status
// indicates whether policy data was successfully read. If it is true, |result|
// contains the value set by policy.
static bool MetricsReportingControlledByPolicy(bool* result) {
  std::wstring key_name = UTF8ToWide(policy::key::kMetricsReportingEnabled);
  DWORD value = 0;
  // TODO(joshia): why hkcu_policy_key opens HKEY_LOCAL_MACHINE?
  base::win::RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE,
                                    policy::kRegistrySubKey, KEY_READ);
  if (hkcu_policy_key.ReadValueDW(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *result = value != 0;
    return true;
  }

  base::win::RegKey hklm_policy_key(HKEY_CURRENT_USER,
                                    policy::kRegistrySubKey, KEY_READ);
  if (hklm_policy_key.ReadValueDW(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *result = value != 0;
    return true;
  }

  return false;
}

static DWORD __stdcall InitCrashReporterThread(void* param) {
  scoped_ptr<CrashReporterInfo> info(
      reinterpret_cast<CrashReporterInfo*>(param));

  // GetCustomInfo can take a few milliseconds to get the file information, so
  // we do it here so it can run in a separate thread.
  info->custom_info = GetCustomInfo(info->dll_path, info->process_type);

  google_breakpad::ExceptionHandler::MinidumpCallback callback = NULL;
  LPTOP_LEVEL_EXCEPTION_FILTER default_filter = NULL;
  // We install the post-dump callback only for the browser and service
  // processes. It spawns a new browser/service process.
  if (info->process_type == L"browser") {
    callback = &DumpDoneCallback;
    default_filter = &ChromeExceptionFilter;
  } else if (info->process_type == L"service") {
    callback = &DumpDoneCallback;
    default_filter = &ServiceExceptionFilter;
  }

  // Check whether configuration management controls crash reporting.
  bool crash_reporting_enabled = true;
  bool controlled_by_policy =
      MetricsReportingControlledByPolicy(&crash_reporting_enabled);

  const CommandLine& command = *CommandLine::ForCurrentProcess();
  bool use_crash_service = !controlled_by_policy &&
      ((command.HasSwitch(switches::kNoErrorDialogs) ||
      GetEnvironmentVariable(
          ASCIIToWide(env_vars::kHeadless).c_str(), NULL, 0)));
  bool is_per_user_install =
      InstallUtil::IsPerUserInstall(info->dll_path.c_str());

  std::wstring pipe_name;
  if (use_crash_service) {
    // Crash reporting is done by crash_service.exe.
    pipe_name = kChromePipeName;
  } else {
    // We want to use the Google Update crash reporting. We need to check if the
    // user allows it first (in case the administrator didn't already decide
    // via policy).
    if (!controlled_by_policy)
      crash_reporting_enabled = GoogleUpdateSettings::GetCollectStatsConsent();

    if (!crash_reporting_enabled) {
      // Configuration managed or the user did not allow Google Update to send
      // crashes, we need to use our default crash handler instead, but only
      // for the browser/service processes.
      if (default_filter)
        InitDefaultCrashCallback(default_filter);
      return 0;
    }

    // Build the pipe name. It can be either:
    // System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
    // Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
    std::wstring user_sid;
    if (is_per_user_install) {
      if (!base::win::GetUserSidString(&user_sid)) {
        if (default_filter)
          InitDefaultCrashCallback(default_filter);
        return -1;
      }
    } else {
      user_sid = kSystemPrincipalSid;
    }

    pipe_name = kGoogleUpdatePipeName;
    pipe_name += user_sid;
  }

  // Get the alternate dump directory. We use the temp path.
  wchar_t temp_dir[MAX_PATH] = {0};
  ::GetTempPathW(MAX_PATH, temp_dir);

  MINIDUMP_TYPE dump_type = kSmallDumpType;
  // Capture full memory if explicitly instructed to.
  if (command.HasSwitch(switches::kFullMemoryCrashReport)) {
    dump_type = kFullDumpType;
  } else {
    // Capture more detail in crash dumps for beta and dev channel builds.
    string16 channel_string;
    GoogleUpdateSettings::GetChromeChannel(!is_per_user_install,
        &channel_string);
    if (channel_string == L"dev" || channel_string == L"beta" ||
        channel_string == GoogleChromeSxSDistribution::ChannelName())
      dump_type = kLargerDumpType;
  }

  g_breakpad = new google_breakpad::ExceptionHandler(temp_dir, &FilterCallback,
                   callback, NULL,
                   google_breakpad::ExceptionHandler::HANDLER_ALL,
                   dump_type, pipe_name.c_str(), info->custom_info);

  if (!g_breakpad->IsOutOfProcess()) {
    // The out-of-process handler is unavailable.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar(env_vars::kNoOOBreakpad, WideToUTF8(info->process_type));
  } else {
    // Tells breakpad to handle breakpoint and single step exceptions.
    // This might break JIT debuggers, but at least it will always
    // generate a crashdump for these exceptions.
    g_breakpad->set_handle_debug_exceptions(true);
  }

  return 0;
}

void InitDefaultCrashCallback(LPTOP_LEVEL_EXCEPTION_FILTER filter) {
  previous_filter = SetUnhandledExceptionFilter(filter);
}

void InitCrashReporterWithDllPath(const std::wstring& dll_path) {
  const CommandLine& command = *CommandLine::ForCurrentProcess();
  if (!command.HasSwitch(switches::kDisableBreakpad)) {
    // Disable the message box for assertions.
    _CrtSetReportMode(_CRT_ASSERT, 0);

    // Query the custom_info now because if we do it in the thread it's going to
    // fail in the sandbox. The thread will delete this object.
    CrashReporterInfo* info(new CrashReporterInfo);
    info->process_type = command.GetSwitchValueNative(switches::kProcessType);
    if (info->process_type.empty())
      info->process_type = L"browser";

    info->dll_path = dll_path;

    // If this is not the browser, we can't be sure that we will be able to
    // initialize the crash_handler in another thread, so we run it right away.
    // This is important to keep the thread for the browser process because
    // it may take some times to initialize the crash_service process.  We use
    // the Windows worker pool to make better reuse of the thread.
    if (info->process_type != L"browser") {
      InitCrashReporterThread(info);
    } else {
      if (QueueUserWorkItem(
              &InitCrashReporterThread,
              info,
              WT_EXECUTELONGFUNCTION) == 0) {
        // We failed to queue to the worker pool, initialize in this thread.
        InitCrashReporterThread(info);
      }
    }
  }
}
