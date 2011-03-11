// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

#include "app/app_paths.h"
#include "app/app_switches.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/i18n/icu_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/metrics/stats_table.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/diagnostics/diagnostics_main.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/profiling.h"
#include "chrome/common/set_process_title.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/common/content_client.h"
#include "content/common/content_paths.h"
#include "content/common/main_function_params.h"
#include "content/common/sandbox_init_wrapper.h"
#include "ipc/ipc_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include <algorithm>
#include <malloc.h>
#include "base/win/registry.h"
#include "sandbox/src/sandbox.h"
#include "tools/memory_watcher/memory_watcher.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/os_crash_dumps.h"
#include "base/mach_ipc_mac.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/browser/mach_broker_mac.h"
#include "chrome/common/chrome_paths_internal.h"
#include "grit/chromium_strings.h"
#include "third_party/WebKit/Source/WebKit/mac/WebCoreSupport/WebSystemInterface.h"
#include "ui/base/l10n/l10n_util_mac.h"
#endif

#if defined(OS_POSIX)
#include <locale.h>
#include <signal.h>
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#endif

#if defined(USE_X11)
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include "ui/base/x/x11_util.h"
#endif

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

extern int BrowserMain(const MainFunctionParams&);
extern int RendererMain(const MainFunctionParams&);
extern int GpuMain(const MainFunctionParams&);
extern int PluginMain(const MainFunctionParams&);
extern int PpapiPluginMain(const MainFunctionParams&);
extern int WorkerMain(const MainFunctionParams&);
extern int NaClMain(const MainFunctionParams&);
extern int UtilityMain(const MainFunctionParams&);
extern int ProfileImportMain(const MainFunctionParams&);
extern int ZygoteMain(const MainFunctionParams&);
#if defined(_WIN64)
extern int NaClBrokerMain(const MainFunctionParams&);
#endif
extern int ServiceProcessMain(const MainFunctionParams&);

#if defined(OS_WIN)
// TODO(erikkay): isn't this already defined somewhere?
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 TCHAR* command_line_unused);
}
#elif defined(OS_POSIX)
extern "C" {
__attribute__((visibility("default")))
int ChromeMain(int argc, char** argv);
}
#endif

namespace {

#if defined(OS_WIN)
const wchar_t kProfilingDll[] = L"memory_watcher.dll";

// Load the memory profiling DLL.  All it needs to be activated
// is to be loaded.  Return true on success, false otherwise.
bool LoadMemoryProfiler() {
  HMODULE prof_module = LoadLibrary(kProfilingDll);
  return prof_module != NULL;
}

// Early versions of Chrome incorrectly registered a chromehtml: URL handler,
// which gives us nothing but trouble. Avoid launching chrome this way since
// some apps fail to properly escape arguments.
bool HasDeprecatedArguments(const std::wstring& command_line) {
  const wchar_t kChromeHtml[] = L"chromehtml:";
  std::wstring command_line_lower = command_line;
  // We are only searching for ASCII characters so this is OK.
  StringToLowerASCII(&command_line_lower);
  std::wstring::size_type pos = command_line_lower.find(kChromeHtml);
  return (pos != std::wstring::npos);
}

#endif  // defined(OS_WIN)

#if defined(OS_LINUX)
static void AdjustLinuxOOMScore(const std::string& process_type) {
  const int kMiscScore = 7;
#if defined(OS_CHROMEOS)
  // On ChromeOS, we want plugins to die after the renderers.  If this
  // works well for ChromeOS, we may do it for Linux as well.
  const int kPluginScore = 4;
#else
  const int kPluginScore = 10;
#endif
  int score = -1;

  if (process_type == switches::kPluginProcess ||
      process_type == switches::kPpapiPluginProcess) {
    score = kPluginScore;
  } else if (process_type == switches::kUtilityProcess ||
             process_type == switches::kWorkerProcess ||
             process_type == switches::kGpuProcess ||
             process_type == switches::kServiceProcess) {
    score = kMiscScore;
  } else if (process_type == switches::kProfileImportProcess) {
    NOTIMPLEMENTED();
#ifndef DISABLE_NACL
  } else if (process_type == switches::kNaClLoaderProcess) {
    score = kPluginScore;
#endif
  } else if (process_type == switches::kZygoteProcess ||
             process_type.empty()) {
    // Pass - browser / zygote process stays at 0.
  } else if (process_type == switches::kExtensionProcess ||
             process_type == switches::kRendererProcess) {
    LOG(WARNING) << "process type '" << process_type << "' "
                 << "should go through the zygote.";
    // When debugging, these process types can end up being run directly.
    return;
  } else {
    NOTREACHED() << "Unknown process type";
  }
  if (score > -1)
    base::AdjustOOMScore(base::GetCurrentProcId(), score);
}
#endif  // defined(OS_LINUX)

void SetupCRT(const CommandLine& command_line) {
#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#else
  if (!command_line.HasSwitch(switches::kDisableBreakpad)) {
    _CrtSetReportMode(_CRT_ASSERT, 0);
  }
#endif
#endif
}

// Enable the heap profiler if the appropriate command-line switch is
// present, bailing out of the app we can't.
void EnableHeapProfiler(const CommandLine& command_line) {
#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kMemoryProfiling))
    if (!LoadMemoryProfiler())
      exit(-1);
#endif
}

void CommonSubprocessInit() {
#if defined(OS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
  MSG msg;
  PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Various things break when you're using a locale where the decimal
  // separator isn't a period.  See e.g. bugs 22782 and 39964.  For
  // all processes except the browser process (where we call system
  // APIs that may rely on the correct locale for formatting numbers
  // when presenting them to the user), reset the locale for numeric
  // formatting.
  // Note that this is not correct for plugin processes -- they can
  // surface UI -- but it's likely they get this wrong too so why not.
  setlocale(LC_NUMERIC, "C");
#endif
#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.  BrowserMain sets
  // this up for the browser process in a different manner.
  InitCrashReporter();
#endif
}

// Returns true if this subprocess type needs the ResourceBundle initialized
// and resources loaded.
bool SubprocessNeedsResourceBundle(const std::string& process_type) {
  return
#if defined(OS_WIN) || defined(OS_MACOSX)
      // Windows needs resources for the default/null plugin.
      // Mac needs them for the plugin process name.
      process_type == switches::kPluginProcess ||
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      // The zygote process opens the resources for the renderers.
      process_type == switches::kZygoteProcess ||
#endif
      process_type == switches::kRendererProcess ||
      process_type == switches::kExtensionProcess ||
      process_type == switches::kUtilityProcess;
}

// Returns true if this process is a child of the browser process.
bool SubprocessIsBrowserChild(const std::string& process_type) {
  if (process_type.empty() || process_type == switches::kServiceProcess) {
    return false;
  }
  return true;
}

#if defined(OS_MACOSX)
// Update the name shown in Activity Monitor so users are less likely to ask
// why Chrome has so many processes.
void SetMacProcessName(const std::string& process_type) {
  // Don't worry about the browser process, its gets the stock name.
  int name_id = 0;
  if (process_type == switches::kRendererProcess) {
    name_id = IDS_RENDERER_APP_NAME;
  } else if (process_type == switches::kPluginProcess) {
    name_id = IDS_PLUGIN_APP_NAME;
  } else if (process_type == switches::kExtensionProcess) {
    name_id = IDS_WORKER_APP_NAME;
  } else if (process_type == switches::kUtilityProcess) {
    name_id = IDS_UTILITY_APP_NAME;
  }
  if (name_id) {
    NSString* app_name = l10n_util::GetNSString(name_id);
    base::mac::SetProcessName(base::mac::NSToCFCast(app_name));
  }
}

// Completes the Mach IPC handshake by sending this process' task port to the
// parent process.  The parent is listening on the Mach port given by
// |GetMachPortName()|.  The task port is used by the parent to get CPU/memory
// stats to display in the task manager.
void SendTaskPortToParentProcess() {
  const mach_msg_timeout_t kTimeoutMs = 100;
  const int32_t kMessageId = 0;
  std::string mach_port_name = MachBroker::GetMachPortName();

  base::MachSendMessage child_message(kMessageId);
  if (!child_message.AddDescriptor(mach_task_self())) {
    LOG(ERROR) << "child AddDescriptor(mach_task_self()) failed.";
    return;
  }

  base::MachPortSender child_sender(mach_port_name.c_str());
  kern_return_t err = child_sender.SendMessage(child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    LOG(ERROR) << StringPrintf("child SendMessage() failed: 0x%x %s", err,
                               mach_error_string(err));
  }
}
#endif  // defined(OS_MACOSX)

void InitializeStatsTable(base::ProcessId browser_pid,
                          const CommandLine& command_line) {
  // Initialize the Stats Counters table.  With this initialized,
  // the StatsViewer can be utilized to read counters outside of
  // Chrome.  These lines can be commented out to effectively turn
  // counters 'off'.  The table is created and exists for the life
  // of the process.  It is not cleaned up.
  if (command_line.HasSwitch(switches::kEnableStatsTable) ||
      command_line.HasSwitch(switches::kEnableBenchmarking)) {
    // NOTIMPLEMENTED: we probably need to shut this down correctly to avoid
    // leaking shared memory regions on posix platforms.
    std::string statsfile =
        StringPrintf("%s-%u", chrome::kStatsFilename,
                     static_cast<unsigned int>(browser_pid));
    base::StatsTable *stats_table = new base::StatsTable(statsfile,
        chrome::kStatsMaxThreads, chrome::kStatsMaxCounters);
    base::StatsTable::set_current(stats_table);
  }
}

#if defined(OS_POSIX)
// Check for --version and --product-version; return true if we encountered
// one of these switches and should exit now.
bool HandleVersionSwitches(const CommandLine& command_line) {
  const chrome::VersionInfo version_info;

#if !defined(OS_MACOSX)
  if (command_line.HasSwitch(switches::kProductVersion)) {
    printf("%s\n", version_info.Version().c_str());
    return true;
  }
#endif

  if (command_line.HasSwitch(switches::kVersion)) {
    printf("%s %s %s\n",
           version_info.Name().c_str(),
           version_info.Version().c_str(),
           platform_util::GetVersionStringModifier().c_str());
    return true;
  }

  return false;
}

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// Show the man page if --help or -h is on the command line.
void HandleHelpSwitches(const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kHelp) ||
      command_line.HasSwitch(switches::kHelpShort)) {
    FilePath binary(command_line.argv()[0]);
    execlp("man", "man", binary.BaseName().value().c_str(), NULL);
    PLOG(FATAL) << "execlp failed";
  }
}
#endif

#endif  // OS_POSIX

// We dispatch to a process-type-specific FooMain() based on a command-line
// flag.  This struct is used to build a table of (flag, main function) pairs.
struct MainFunction {
  const char* name;
  int (*function)(const MainFunctionParams&);
};

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// On platforms that use the zygote, we have a special subset of
// subprocesses that are launched via the zygote.  This function
// fills in some process-launching bits around ZygoteMain().
// Returns the exit code of the subprocess.
int RunZygote(const MainFunctionParams& main_function_params) {
  static const MainFunction kMainFunctions[] = {
    { switches::kRendererProcess,    RendererMain },
    { switches::kExtensionProcess,   RendererMain },
    { switches::kWorkerProcess,      WorkerMain },
    { switches::kPpapiPluginProcess, PpapiPluginMain },
#if !defined(DISABLE_NACL)
    { switches::kNaClLoaderProcess,  NaClMain },
#endif
  };

  // This function call can return multiple times, once per fork().
  if (!ZygoteMain(main_function_params))
    return 1;

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // The StatsTable must be initialized in each process; we already
  // initialized for the browser process, now we need to initialize
  // within the new processes as well.
  pid_t browser_pid = base::GetParentProcessId(
      base::GetParentProcessId(base::GetCurrentProcId()));
  InitializeStatsTable(browser_pid, command_line);

  MainFunctionParams main_params(command_line,
                                 main_function_params.sandbox_info_,
                                 main_function_params.autorelease_pool_);
  // Get the new process type from the new command line.
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_params);
  }

  NOTREACHED() << "Unknown zygote process type: " << process_type;
  return 1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

// Run the FooMain() for a given process type.
// If |process_type| is empty, runs BrowserMain().
// Returns the exit code for this process.
int RunNamedProcessTypeMain(const std::string& process_type,
                            const MainFunctionParams& main_function_params) {
  static const MainFunction kMainFunctions[] = {
    { "",                            BrowserMain },
    { switches::kRendererProcess,    RendererMain },
    // An extension process is just a renderer process. We use a different
    // command line argument to differentiate crash reports.
    { switches::kExtensionProcess,   RendererMain },
    { switches::kPluginProcess,      PluginMain },
    { switches::kWorkerProcess,      WorkerMain },
    { switches::kPpapiPluginProcess, PpapiPluginMain },
    { switches::kUtilityProcess,     UtilityMain },
    { switches::kGpuProcess,         GpuMain },
    { switches::kServiceProcess,     ServiceProcessMain },

#if defined(OS_MACOSX)
    // TODO(port): Use OOP profile import - http://crbug.com/22142 .
    { switches::kProfileImportProcess, ProfileImportMain },
#endif
#if !defined(DISABLE_NACL)
    { switches::kNaClLoaderProcess, NaClMain },
#ifdef _WIN64  // The broker process is used only on Win64.
    { switches::kNaClBrokerProcess, NaClBrokerMain },
#endif
#endif  // DISABLE_NACL
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    // Zygote startup is special -- see RunZygote comments above
    // for why we don't use ZygoteMain directly.
    { switches::kZygoteProcess, RunZygote },
#endif
  };

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_function_params);
  }

  NOTREACHED() << "Unknown process type: " << process_type;
  return 1;
}

}  // namespace

#if defined(OS_WIN)
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 TCHAR* command_line_unused) {
  // argc/argv are ignored on Windows; see command_line.h for details.
  int argc = 0;
  char** argv = NULL;
#elif defined(OS_POSIX)
int ChromeMain(int argc, char** argv) {
  // There is no HINSTANCE on non-Windows.
  void* instance = NULL;
#endif
  // LowLevelInit performs startup initialization before we
  // e.g. allocate any memory.  It must be the first call on startup.
  chrome_main::LowLevelInit(instance);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  // We need this pool for all the objects created before we get to the
  // event loop, but we don't want to leave them hanging around until the
  // app quits. Each "main" needs to flush this pool right before it goes into
  // its main event loop to get rid of the cruft.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->SaveChromeMainStats();
#endif

  CommandLine::Init(argc, argv);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  Profiling::ProcessStarted();

#if defined(OS_POSIX)
  if (HandleVersionSwitches(command_line))
    return 0;  // Got a --version switch; exit with a success error code.
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  // This will directly exit if the user asked for help.
  HandleHelpSwitches(command_line);
#endif
#endif  // OS_POSIX

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_MACOSX)
  base::mac::SetOverrideAppBundlePath(chrome::GetFrameworkBundlePath());
#endif  // OS_MACOSX

  // If we are in diagnostics mode this is the end of the line. After the
  // diagnostics are run the process will invariably exit.
  if (command_line.HasSwitch(switches::kDiagnostics)) {
    return DiagnosticsMain(command_line);
  }

#if defined(OS_WIN)
  // Must do this before any other usage of command line!
  if (HasDeprecatedArguments(command_line.command_line_string()))
    return 1;
#endif

  base::ProcessId browser_pid = base::GetCurrentProcId();
  if (SubprocessIsBrowserChild(process_type)) {
#if defined(OS_WIN) || defined(OS_MACOSX)
    std::string channel_name =
        command_line.GetSwitchValueASCII(switches::kProcessChannelID);

    int browser_pid_int;
    base::StringToInt(channel_name, &browser_pid_int);
    browser_pid = static_cast<base::ProcessId>(browser_pid_int);
    DCHECK_NE(browser_pid_int, 0);
#elif defined(OS_POSIX)
    // On linux, we're in the zygote here; so we need the parent process' id.
    browser_pid = base::GetParentProcessId(base::GetCurrentProcId());
#endif

#if defined(OS_MACOSX)
    SendTaskPortToParentProcess();
#endif

#if defined(OS_POSIX)
    // When you hit Ctrl-C in a terminal running the browser
    // process, a SIGINT is delivered to the entire process group.
    // When debugging the browser process via gdb, gdb catches the
    // SIGINT for the browser process (and dumps you back to the gdb
    // console) but doesn't for the child processes, killing them.
    // The fix is to have child processes ignore SIGINT; they'll die
    // on their own when the browser process goes away.
    //
    // Note that we *can't* rely on BeingDebugged to catch this case because we
    // are the child process, which is not being debugged.
    // TODO(evanm): move this to some shared subprocess-init function.
    if (!base::debug::BeingDebugged())
      signal(SIGINT, SIG_IGN);
#endif
  }
  SetupCRT(command_line);

#if defined(USE_NSS)
  base::EarlySetupForNSSInit();
#endif

  // Initialize the Chrome path provider.
  app::RegisterPathProvider();
  ui::RegisterPathProvider();
  chrome::RegisterPathProvider();
  content::RegisterPathProvider();

#if defined(OS_MACOSX)
  // On the Mac, the child executable lives at a predefined location within
  // the app bundle's versioned directory.
  PathService::Override(content::CHILD_PROCESS_EXE,
      chrome::GetVersionedDirectory().
          Append(chrome::kHelperProcessExecutablePath));
#endif

  // Initialize the content client which that code uses to talk to Chrome.
  chrome::ChromeContentClient chrome_content_client;
  content::SetContentClient(&chrome_content_client);

  // Notice a user data directory override if any
  FilePath user_data_dir =
      command_line.GetSwitchValuePath(switches::kUserDataDir);
  chrome_main::CheckUserDataDirPolicy(&user_data_dir);
  if (!user_data_dir.empty())
    CHECK(PathService::Override(chrome::DIR_USER_DATA, user_data_dir));

#if defined(OS_MACOSX)
  // TODO(mark): Right now, InitCrashReporter() needs to be called after
  // CommandLine::Init() and chrome::RegisterPathProvider().  Ideally, Breakpad
  // initialization could occur sooner, preferably even before the framework
  // dylib is even loaded, to catch potential early crashes.
  InitCrashReporter();

#if defined(NDEBUG)
  bool is_debug_build = false;
#else
  bool is_debug_build = true;
#endif

  // Details on when we enable Apple's Crash reporter.
  //
  // Motivation:
  //    In debug mode it takes Apple's crash reporter eons to generate a crash
  // dump.
  //
  // What we do:
  // * We only pass crashes for foreground processes to Apple's Crash reporter.
  //    At the time of this writing, that means just the Browser process.
  // * If Breakpad is enabled, it will pass browser crashes to Crash Reporter
  //    itself.
  // * If Breakpad is disabled, we only turn on Crash Reporter for the
  //    Browser process in release mode.
  if (!command_line.HasSwitch(switches::kDisableBreakpad)) {
    bool disable_apple_crash_reporter = is_debug_build
                                        || base::mac::IsBackgroundOnlyProcess();
    if (!IsCrashReporterEnabled() && disable_apple_crash_reporter) {
      base::mac::DisableOSCrashDumps();
    }
  }

  // Mac Chrome is packaged with a main app bundle and a helper app bundle.
  // The main app bundle should only be used for the browser process, so it
  // should never see a --type switch (switches::kProcessType).  Likewise,
  // the helper should always have a --type switch.
  //
  // This check is done this late so there is already a call to
  // base::mac::IsBackgroundOnlyProcess(), so there is no change in
  // startup/initialization order.

  // The helper's Info.plist marks it as a background only app.
  if (base::mac::IsBackgroundOnlyProcess()) {
    CHECK(command_line.HasSwitch(switches::kProcessType))
        << "Helper application requires --type.";
  } else {
    CHECK(!command_line.HasSwitch(switches::kProcessType))
        << "Main application forbids --type, saw \"" << process_type << "\".";
  }

  if (IsCrashReporterEnabled())
    InitCrashProcessInfo();
#endif  // defined(OS_MACOSX)

  InitializeStatsTable(browser_pid, command_line);

  base::StatsScope<base::StatsCounterTimer>
      startup_timer(chrome::Counters::chrome_main());

  // Enable the heap profiler as early as possible!
  EnableHeapProfiler(command_line);

  // Enable Message Loop related state asap.
  if (command_line.HasSwitch(switches::kMessageLoopHistogrammer))
    MessageLoop::EnableHistogrammer(true);

  bool single_process =
#if defined (GOOGLE_CHROME_BUILD)
    // This is an unsupported and not fully tested mode, so don't enable it for
    // official Chrome builds.
    false;
#else
    command_line.HasSwitch(switches::kSingleProcess);
#endif
  if (single_process)
    RenderProcessHost::set_run_renderer_in_process(true);
#if defined(OS_MACOSX)
  // TODO(port-mac): This is from renderer_main_platform_delegate.cc.
  // shess tried to refactor things appropriately, but it sprawled out
  // of control because different platforms needed different styles of
  // initialization.  Try again once we understand the process
  // architecture needed and where it should live.
  if (single_process)
    InitWebCoreSystemInterface();
#endif

  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);

  logging::OldFileDeletionState file_state =
      logging::APPEND_TO_OLD_LOG_FILE;
  if (process_type.empty()) {
    file_state = logging::DELETE_OLD_LOG_FILE;
  }
  logging::InitChromeLogging(command_line, file_state);

  // Register internal Chrome schemes so they'll be parsed correctly. This must
  // happen before we process any URLs with the affected schemes, and must be
  // done in all processes that work with these URLs (i.e. including renderers).
  chrome::RegisterChromeSchemes();

  if (SubprocessNeedsResourceBundle(process_type)) {
    // Initialize ResourceBundle which handles files loaded from external
    // sources.  The language should have been passed in to us from the
    // browser process as a command line flag.
    DCHECK(command_line.HasSwitch(switches::kLang) ||
           process_type == switches::kZygoteProcess);

    // TODO(markusheintz): The command line flag --lang is actually processed
    // by the CommandLinePrefStore, and made available through the PrefService
    // via the preference prefs::kApplicationLocale. The browser process uses
    // the --lang flag to pass the value of the PrefService in here. Maybe this
    // value could be passed in a different way.
    const std::string locale =
        command_line.GetSwitchValueASCII(switches::kLang);
    const std::string loaded_locale =
        ResourceBundle::InitSharedInstance(locale);
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " << locale;

#if defined(OS_MACOSX)
    // Update the process name (need resources to get the strings, so
    // only do this when ResourcesBundle has been initialized).
    SetMacProcessName(process_type);
#endif  // defined(OS_MACOSX)
  }

  if (!process_type.empty())
    CommonSubprocessInit();

  // Initialize the sandbox for this process.
  SandboxInitWrapper sandbox_wrapper;
  bool initialize_sandbox = true;

#if defined(OS_WIN)
  sandbox_wrapper.SetServices(sandbox_info);
#elif defined(OS_MACOSX)
  // On OS X the renderer sandbox needs to be initialized later in the startup
  // sequence in RendererMainPlatformDelegate::EnableSandbox().
  // Same goes for NaClLoader, in NaClMainPlatformDelegate::EnableSandbox(),
  // and the GPU process, in GpuMain().
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kExtensionProcess ||
      process_type == switches::kNaClLoaderProcess ||
      process_type == switches::kGpuProcess) {
    initialize_sandbox = false;
  }
#endif

  if (initialize_sandbox) {
    bool sandbox_initialized_ok =
        sandbox_wrapper.InitializeSandbox(command_line, process_type);
    // Die if the sandbox can't be enabled.
    CHECK(sandbox_initialized_ok) << "Error initializing sandbox for "
                                  << process_type;
  }

  startup_timer.Stop();  // End of Startup Time Measurement.

  MainFunctionParams main_params(command_line, sandbox_wrapper,
                                 &autorelease_pool);

  // Note: If you are adding a new process type below, be sure to adjust the
  // AdjustLinuxOOMScore function too.
#if defined(OS_LINUX)
  AdjustLinuxOOMScore(process_type);
  // TODO(mdm): look into calling CommandLine::SetProcTitle() here instead of
  // in each relevant main() function below, to fix /proc/self/exe showing up
  // as our process name since we exec() via that to be update-safe.
#endif

#if defined(OS_POSIX)
  SetProcessTitleFromCommandLine(argv);
#endif

  int exit_code = RunNamedProcessTypeMain(process_type, main_params);

  if (SubprocessNeedsResourceBundle(process_type))
    ResourceBundle::CleanupSharedInstance();

  logging::CleanupChromeLogging();

  chrome_main::LowLevelShutdown();

  return exit_code;
}
