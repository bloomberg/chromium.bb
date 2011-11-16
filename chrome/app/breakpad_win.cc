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
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/app/hard_error_handler_win.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
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
static size_t g_num_switches_offset;
static size_t g_switches_offset;

// Maximum length for plugin path to include in plugin crash reports.
const size_t kMaxPluginPathLength = 256;

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

static void SetIntegerValue(size_t offset, int value) {
  if (!g_custom_entries)
    return;

  base::wcslcpy((*g_custom_entries)[offset].value,
                base::StringPrintf(L"%d", value).c_str(),
                google_breakpad::CustomInfoEntry::kValueMaxLength);
}

bool IsBoringCommandLineSwitch(const std::wstring& flag) {
  return StartsWith(flag, L"--channel=", true) ||

         // No point to including this since we already have a ptype field.
         StartsWith(flag, L"--type=", true) ||

         // Not particularly interesting
         StartsWith(flag, L"--flash-broker=", true) ||

         // Just about everything has this, don't bother.
         StartsWith(flag, L"/prefetch:", true) ||

         // We handle the plugin path separately since it is usually too big
         // to fit in the switches (limited to 63 characters).
         StartsWith(flag, L"--plugin-path=", true) ||

         // This is too big so we end up truncating it anyway.
         StartsWith(flag, L"--force-fieldtest=", true) ||

         // These surround the flags that were added by about:flags, it lets
         // you distinguish which flags were added manually via the command
         // line versus those added through about:flags. For the most part
         // we don't care how an option was enabled, so we strip these.
         // (If you need to know can always look at the PEB).
         flag == L"--flag-switches-begin" ||
         flag == L"--flag-switches-end";
}

extern "C" void __declspec(dllexport) __cdecl SetCommandLine(
    const CommandLine* command_line) {
  if (!g_custom_entries)
    return;

  const CommandLine::StringVector& argv = command_line->argv();

  // Copy up to the kMaxSwitches arguments into the custom entries array. Skip
  // past the first argument, as it is just the executable path.
  size_t argv_i = 1;
  size_t num_added = 0;

  for (; argv_i < argv.size() && num_added < kMaxSwitches; ++argv_i) {
    // Don't bother including boring command line switches in crash reports.
    if (IsBoringCommandLineSwitch(argv[argv_i]))
      continue;

    base::wcslcpy((*g_custom_entries)[g_switches_offset + num_added].value,
                  argv[argv_i].c_str(),
                  google_breakpad::CustomInfoEntry::kValueMaxLength);
    num_added++;
  }

  // Make note of the total number of switches. This is useful in case we have
  // truncated at kMaxSwitches, to see how many were unaccounted for.
  SetIntegerValue(g_num_switches_offset, static_cast<int>(argv.size()) - 1);
}

// Appends the plugin path to |g_custom_entries|.
void SetPluginPath(const std::wstring& path) {
  DCHECK(g_custom_entries);

  if (path.size() > kMaxPluginPathLength) {
    // If the path is too long, truncate from the start rather than the end,
    // since we want to be able to recover the DLL name.
    SetPluginPath(path.substr(path.size() - kMaxPluginPathLength));
    return;
  }

  // The chunk size without terminator.
  const size_t kChunkSize = static_cast<size_t>(
      google_breakpad::CustomInfoEntry::kValueMaxLength - 1);

  int chunk_index = 0;
  size_t chunk_start = 0;  // Current position inside |path|

  for (chunk_start = 0; chunk_start < path.size(); chunk_index++) {
    size_t chunk_length = std::min(kChunkSize, path.size() - chunk_start);

    g_custom_entries->push_back(google_breakpad::CustomInfoEntry(
        base::StringPrintf(L"plugin-path-chunk-%i", chunk_index + 1).c_str(),
        path.substr(chunk_start, chunk_length).c_str()));

    chunk_start += chunk_length;
  }
}

// Returns the custom info structure based on the dll in parameter and the
// process type.
google_breakpad::CustomClientInfo* GetCustomInfo(const std::wstring& dll_path,
                                                 const std::wstring& type,
                                                 const std::wstring& channel) {
  scoped_ptr<FileVersionInfo>
      version_info(FileVersionInfo::CreateFileVersionInfo(FilePath(dll_path)));

  std::wstring version, product;
  std::wstring special_build;
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

    special_build = version_info->special_build();
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
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"channel", channel.c_str()));

  if (!special_build.empty())
    g_custom_entries->push_back(
        google_breakpad::CustomInfoEntry(L"special", special_build.c_str()));

  g_num_of_extensions_offset = g_custom_entries->size();
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"num-extensions", L"N/A"));

  g_extension_ids_offset = g_custom_entries->size();
  for (int i = 0; i < kMaxReportedActiveExtensions; ++i) {
    g_custom_entries->push_back(google_breakpad::CustomInfoEntry(
        base::StringPrintf(L"extension-%i", i + 1).c_str(), L""));
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

  // Add empty values for the command line switches. We will fill them with
  // actual values as part of SetCommandLine().
  g_num_switches_offset = g_custom_entries->size();
  g_custom_entries->push_back(
      google_breakpad::CustomInfoEntry(L"num-switches", L""));

  g_switches_offset = g_custom_entries->size();
  for (int i = 0; i < kMaxSwitches; ++i) {
    g_custom_entries->push_back(google_breakpad::CustomInfoEntry(
        base::StringPrintf(L"switch-%i", i + 1).c_str(), L""));
  }

  // Fill in the command line arguments using CommandLine::ForCurrentProcess().
  // The browser process may call SetCommandLine() again later on with a command
  // line that has been augmented with the about:flags experiments.
  SetCommandLine(CommandLine::ForCurrentProcess());

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
          base::StringPrintf(L"url-chunk-%i", i + 1).c_str(), L""));
    }

    if (type == L"plugin") {
      std::wstring plugin_path =
          CommandLine::ForCurrentProcess()->GetSwitchValueNative("plugin-path");
      if (!plugin_path.empty())
        SetPluginPath(plugin_path);
    }
  } else {
    g_custom_entries->push_back(
        google_breakpad::CustomInfoEntry(L"num-views", L"N/A"));
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

  base::wcslcpy((*g_custom_entries)[g_client_id_offset].value,
                client_id,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
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

  base::wcslcpy((*g_custom_entries)[g_extension_ids_offset + index].value,
                id,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
}

extern "C" void __declspec(dllexport) __cdecl SetGpuInfo(
    const wchar_t* vendor_id, const wchar_t* device_id,
    const wchar_t* driver_version, const wchar_t* pixel_shader_version,
    const wchar_t* vertex_shader_version) {
  if (!g_custom_entries)
    return;

  base::wcslcpy((*g_custom_entries)[g_gpu_info_offset].value,
                vendor_id,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
  base::wcslcpy((*g_custom_entries)[g_gpu_info_offset+1].value,
                device_id,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
  base::wcslcpy((*g_custom_entries)[g_gpu_info_offset+2].value,
                driver_version,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
  base::wcslcpy((*g_custom_entries)[g_gpu_info_offset+3].value,
                pixel_shader_version,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
  base::wcslcpy((*g_custom_entries)[g_gpu_info_offset+4].value,
                vertex_shader_version,
                google_breakpad::CustomInfoEntry::kValueMaxLength);
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
    ::ExitProcess(chrome::RESULT_CODE_RESPAWN_FAILED);
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
  // See ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment()
  // for details.
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

// Crashes the process after generating a dump for the provided exception. Note
// that the crash reporter should be initialized before calling this function
// for it to do anything.
extern "C" int __declspec(dllexport) CrashForException(
    EXCEPTION_POINTERS* info) {
  if (g_breakpad) {
    g_breakpad->WriteMinidumpForException(info);
    ::ExitProcess(content::RESULT_CODE_KILLED);
  }
  return EXCEPTION_CONTINUE_SEARCH;
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

  bool is_per_user_install =
      InstallUtil::IsPerUserInstall(info->dll_path.c_str());

  std::wstring channel_string;
  GoogleUpdateSettings::GetChromeChannelAndModifiers(!is_per_user_install,
                                                     &channel_string);

  // GetCustomInfo can take a few milliseconds to get the file information, so
  // we do it here so it can run in a separate thread.
  info->custom_info = GetCustomInfo(info->dll_path, info->process_type,
                                    channel_string);

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

  std::wstring pipe_name;
  if (use_crash_service) {
    // Crash reporting is done by crash_service.exe.
    const wchar_t* var_name = L"CHROME_BREAKPAD_PIPE_NAME";
    DWORD value_length = ::GetEnvironmentVariableW(var_name, NULL, 0);
    if (value_length == 0) {
      pipe_name = kChromePipeName;
    } else {
      scoped_array<wchar_t> value(new wchar_t[value_length]);
      ::GetEnvironmentVariableW(var_name, value.get(), value_length);
      pipe_name = value.get();
    }
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
#ifdef _WIN64
  pipe_name += L"-x64";
#endif

  // Get the alternate dump directory. We use the temp path.
  wchar_t temp_dir[MAX_PATH] = {0};
  ::GetTempPathW(MAX_PATH, temp_dir);

  MINIDUMP_TYPE dump_type = kSmallDumpType;
  // Capture full memory if explicitly instructed to.
  if (command.HasSwitch(switches::kFullMemoryCrashReport)) {
    dump_type = kFullDumpType;
  } else {
    std::wstring channel_name(
        GoogleUpdateSettings::GetChromeChannel(!is_per_user_install));

    // Capture more detail in crash dumps for beta and dev channel builds.
    if (channel_name == L"dev" || channel_name == L"beta" ||
        channel_name == GoogleChromeSxSDistribution::ChannelName())
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
