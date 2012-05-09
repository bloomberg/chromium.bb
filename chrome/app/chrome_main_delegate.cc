// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_delegate.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/diagnostics/diagnostics_main.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/profiling.h"
#include "chrome/common/url_constants.h"
#include "chrome/plugin/chrome_content_plugin_client.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/common/content_counters.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include <algorithm>
#include <atlbase.h>
#include <malloc.h>
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "policy/policy_constants.h"
#include "sandbox/src/sandbox.h"
#include "tools/memory_watcher/memory_watcher.h"
#include "ui/base/resource/resource_bundle_win.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/os_crash_dumps.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/app/chrome_main_mac.h"
#include "chrome/browser/mac/relauncher.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/mac/cfbundle_blocker.h"
#include "chrome/common/mac/objc_zombie.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#endif

#if defined(OS_POSIX)
#include <locale.h>
#include <signal.h>
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "chrome/app/nacl_fork_delegate_linux.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#endif

#if defined(USE_X11)
#include <stdlib.h>
#include <string.h>
#include "ui/base/x/x11_util.h"
#endif

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linuxish.h"
#endif

base::LazyInstance<chrome::ChromeContentBrowserClient>
    g_chrome_content_browser_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<chrome::ChromeContentRendererClient>
    g_chrome_content_renderer_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<chrome::ChromeContentUtilityClient>
    g_chrome_content_utility_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<chrome::ChromeContentPluginClient>
    g_chrome_content_plugin_client = LAZY_INSTANCE_INITIALIZER;

extern int NaClMain(const content::MainFunctionParams&);
extern int ServiceProcessMain(const content::MainFunctionParams&);

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

// Checks if the registry key exists in the given hive and expands any
// variables in the string.
bool LoadUserDataDirPolicyFromRegistry(HKEY hive,
                                       const std::wstring& key_name,
                                       FilePath* user_data_dir) {
  std::wstring value;

  base::win::RegKey policy_key(hive,
                               policy::kRegistryMandatorySubKey,
                               KEY_READ);
  if (policy_key.ReadValue(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *user_data_dir = FilePath(policy::path_parser::ExpandPathVariables(value));
    return true;
  }
  return false;
}

void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  DCHECK(user_data_dir);
  // We are running as Chrome Frame if we were invoked with user-data-dir,
  // chrome-frame, and automation-channel switches.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const bool is_chrome_frame =
      !user_data_dir->empty() &&
      command_line->HasSwitch(switches::kChromeFrame) &&
      command_line->HasSwitch(switches::kAutomationClientChannelID);

  // In the case of Chrome Frame, the last path component of the user-data-dir
  // provided on the command line must be preserved since it is specific to
  // CF's host.
  FilePath cf_host_dir;
  if (is_chrome_frame)
    cf_host_dir = user_data_dir->BaseName();

  // Policy from the HKLM hive has precedence over HKCU so if we have one here
  // we don't have to try to load HKCU.
  const char* key_name_ascii = (is_chrome_frame ? policy::key::kGCFUserDataDir :
                                policy::key::kUserDataDir);
  std::wstring key_name(ASCIIToWide(key_name_ascii));
  if (LoadUserDataDirPolicyFromRegistry(HKEY_LOCAL_MACHINE, key_name,
                                        user_data_dir) ||
      LoadUserDataDirPolicyFromRegistry(HKEY_CURRENT_USER, key_name,
                                        user_data_dir)) {
    // A Group Policy value was loaded.  Append the Chrome Frame host directory
    // if relevant.
    if (is_chrome_frame)
      *user_data_dir = user_data_dir->Append(cf_host_dir);
  }
}

// If we try to access a path that is not currently available, we want the call
// to fail rather than show an error dialog.
void SuppressWindowsErrorDialogs() {
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode.
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
}

#endif  // defined(OS_WIN)

#if defined(OS_LINUX)
static void AdjustLinuxOOMScore(const std::string& process_type) {
  // Browsers and zygotes should still be killable, but killed last.
  const int kZygoteScore = 0;
  // The minimum amount to bump a score by.  This is large enough that
  // even if it's translated into the old values, it will still go up
  // by at least one.
  const int kScoreBump = 100;
  // This is the lowest score that renderers and extensions start with
  // in the OomPriorityManager.
  const int kRendererScore = chrome::kLowestRendererOomScore;
  // For "miscellaneous" things, we want them after renderers,
  // but before plugins.
  const int kMiscScore = kRendererScore - kScoreBump;
  // We want plugins to die after the renderers.
  const int kPluginScore = kMiscScore - kScoreBump;
  int score = -1;

  DCHECK(kMiscScore > 0);
  DCHECK(kPluginScore > 0);

  if (process_type == switches::kPluginProcess ||
      process_type == switches::kPpapiPluginProcess) {
    score = kPluginScore;
  } else if (process_type == switches::kPpapiBrokerProcess) {
    // The broker should be killed before the PPAPI plugin.
    score = kPluginScore + kScoreBump;
  } else if (process_type == switches::kUtilityProcess ||
             process_type == switches::kWorkerProcess ||
             process_type == switches::kGpuProcess ||
             process_type == switches::kServiceProcess) {
    score = kMiscScore;
#ifndef DISABLE_NACL
  } else if (process_type == switches::kNaClLoaderProcess) {
    score = kPluginScore;
#endif
  } else if (process_type == switches::kZygoteProcess ||
             process_type.empty()) {
    // For zygotes and unlabeled process types, we want to still make
    // them killable by the OOM killer.
    score = kZygoteScore;
  } else if (process_type == switches::kRendererProcess) {
    LOG(WARNING) << "process type 'renderer' "
                 << "should be created through the zygote.";
    // When debugging, this process type can end up being run directly, but
    // this isn't the typical path for assigning the OOM score for it.  Still,
    // we want to assign a score that is somewhat representative for debugging.
    score = kRendererScore;
  } else {
    NOTREACHED() << "Unknown process type";
  }
  if (score > -1)
    base::AdjustOOMScore(base::GetCurrentProcId(), score);
}
#endif  // defined(OS_LINUX)

// Enable the heap profiler if the appropriate command-line switch is
// present, bailing out of the app we can't.
void EnableHeapProfiler(const CommandLine& command_line) {
#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kMemoryProfiling))
    if (!LoadMemoryProfiler())
      exit(-1);
#endif
}

void InitializeChromeContentRendererClient() {
  content::GetContentClient()->set_renderer(
      &g_chrome_content_renderer_client.Get());
}

void InitializeChromeContentClient(const std::string& process_type) {
  if (process_type.empty()) {
    content::GetContentClient()->set_browser(
        &g_chrome_content_browser_client.Get());
  } else if (process_type == switches::kPluginProcess) {
    content::GetContentClient()->set_plugin(
        &g_chrome_content_plugin_client.Get());
  } else if (process_type == switches::kRendererProcess) {
    InitializeChromeContentRendererClient();
  } else if (process_type == switches::kUtilityProcess) {
    content::GetContentClient()->set_utility(
        &g_chrome_content_utility_client.Get());
  }
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
#if defined(OS_MACOSX)
      // Mac needs them too for scrollbar related images and for sandbox
      // profiles.
      process_type == switches::kWorkerProcess ||
      process_type == switches::kNaClLoaderProcess ||
      process_type == switches::kPpapiPluginProcess ||
      process_type == switches::kPpapiBrokerProcess ||
      process_type == switches::kGpuProcess ||
#endif
      process_type == switches::kRendererProcess ||
      process_type == switches::kUtilityProcess;
}

#if defined(OS_MACOSX)
// Update the name shown in Activity Monitor so users are less likely to ask
// why Chrome has so many processes.
void SetMacProcessName(const CommandLine& command_line) {
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  // Don't worry about the browser process, its gets the stock name.
  int name_id = 0;
  if (command_line.HasSwitch(switches::kExtensionProcess)) {
    name_id = IDS_WORKER_APP_NAME;
  } else if (process_type == switches::kRendererProcess) {
    name_id = IDS_RENDERER_APP_NAME;
  } else if (process_type == switches::kPluginProcess ||
             process_type == switches::kPpapiPluginProcess) {
    name_id = IDS_PLUGIN_APP_NAME;
  } else if (process_type == switches::kUtilityProcess) {
    name_id = IDS_UTILITY_APP_NAME;
  }
  if (name_id) {
    NSString* app_name = l10n_util::GetNSString(name_id);
    base::mac::SetProcessName(base::mac::NSToCFCast(app_name));
  }
}

#endif  // defined(OS_MACOSX)

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
           chrome::VersionInfo::GetVersionStringModifier().c_str());
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

#if !defined(OS_MACOSX)
void SIGTERMProfilingShutdown(int signal) {
  Profiling::Stop();
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGTERM, &sigact, NULL) == 0);
  raise(signal);
}

void SetUpProfilingShutdownHandler() {
  struct sigaction sigact;
  sigact.sa_handler = SIGTERMProfilingShutdown;
  sigact.sa_flags = SA_RESETHAND;
  sigemptyset(&sigact.sa_mask);
  CHECK(sigaction(SIGTERM, &sigact, NULL) == 0);
}
#endif

#endif  // OS_POSIX

struct MainFunction {
  const char* name;
  int (*function)(const content::MainFunctionParams&);
};

}  // namespace

ChromeMainDelegate::ChromeMainDelegate() {
}

ChromeMainDelegate::~ChromeMainDelegate() {
}

bool ChromeMainDelegate::BasicStartupComplete(int* exit_code) {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->SaveChromeMainStats();
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

#if defined(OS_MACOSX)
  // Give the browser process a longer treadmill, since crashes
  // there have more impact.
  const bool is_browser = !command_line.HasSwitch(switches::kProcessType);
  ObjcEvilDoers::ZombieEnable(true, is_browser ? 10000 : 1000);

  SetUpBundleOverrides();
  chrome::common::mac::EnableCFBundleBlocker();
#endif

  Profiling::ProcessStarted();

#if defined(OS_POSIX)
  if (HandleVersionSwitches(command_line)) {
    *exit_code = 0;
    return true;  // Got a --version switch; exit with a success error code.
  }
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  // This will directly exit if the user asked for help.
  HandleHelpSwitches(command_line);
#endif
#endif  // OS_POSIX

  // If we are in diagnostics mode this is the end of the line. After the
  // diagnostics are run the process will invariably exit.
  if (command_line.HasSwitch(switches::kDiagnostics)) {
    *exit_code = DiagnosticsMain(command_line);
    return true;
  }

#if defined(OS_WIN)
  // Must do this before any other usage of command line!
  if (HasDeprecatedArguments(command_line.GetCommandLineString())) {
    *exit_code = 1;
    return true;
  }
#endif

  if (!command_line.HasSwitch(switches::kProcessType) &&
      command_line.HasSwitch(switches::kEnableBenchmarking)) {
    base::FieldTrial::EnableBenchmarking();
  }

  std::string process_type =  command_line.GetSwitchValueASCII(
      switches::kProcessType);
  content::SetContentClient(&chrome_content_client_);
  InitializeChromeContentClient(process_type);

  return false;
}

#if defined(OS_MACOSX)
void ChromeMainDelegate::InitMacCrashReporter(const CommandLine& command_line,
                                              const std::string& process_type) {
  // TODO(mark): Right now, InitCrashReporter() needs to be called after
  // CommandLine::Init() and chrome::RegisterPathProvider().  Ideally,
  // Breakpad initialization could occur sooner, preferably even before the
  // framework dylib is even loaded, to catch potential early crashes.
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
  // * We only pass crashes for foreground processes to Apple's Crash
  //    reporter. At the time of this writing, that means just the Browser
  //    process.
  // * If Breakpad is enabled, it will pass browser crashes to Crash Reporter
  //    itself.
  // * If Breakpad is disabled, we only turn on Crash Reporter for the
  //    Browser process in release mode.
  if (!command_line.HasSwitch(switches::kDisableBreakpad)) {
    bool disable_apple_crash_reporter = is_debug_build ||
        base::mac::IsBackgroundOnlyProcess();
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
    CHECK(command_line.HasSwitch(switches::kProcessType) &&
          !process_type.empty())
        << "Helper application requires --type.";

    // In addition, some helper flavors only work with certain process types.
    FilePath executable;
    if (PathService::Get(base::FILE_EXE, &executable) &&
        executable.value().size() >= 3) {
      std::string last_three =
          executable.value().substr(executable.value().size() - 3);

      if (last_three == " EH") {
        CHECK(process_type == switches::kPluginProcess ||
              process_type == switches::kUtilityProcess)
            << "Executable-heap process requires --type="
            << switches::kPluginProcess << " or "
            << switches::kUtilityProcess << ", saw " << process_type;
      } else if (last_three == " NP") {
        CHECK_EQ(switches::kNaClLoaderProcess, process_type)
            << "Non-PIE process requires --type="
            << switches::kNaClLoaderProcess << ", saw " << process_type;
      } else {
        CHECK(process_type != switches::kPluginProcess &&
              process_type != switches::kNaClLoaderProcess)
            << "Non-executable-heap PIE process is intolerant of --type="
            << switches::kPluginProcess << " and "
            << switches::kNaClLoaderProcess << ", saw " << process_type;
      }
    }
  } else {
    CHECK(!command_line.HasSwitch(switches::kProcessType) &&
          process_type.empty())
        << "Main application forbids --type, saw " << process_type;
  }

  if (IsCrashReporterEnabled())
    InitCrashProcessInfo();
}
#endif  // defined(OS_MACOSX)

void ChromeMainDelegate::PreSandboxStartup() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  chrome::RegisterPathProvider();

#if defined(OS_MACOSX)
  // On the Mac, the child executable lives at a predefined location within
  // the app bundle's versioned directory.
  PathService::Override(content::CHILD_PROCESS_EXE,
                        chrome::GetVersionedDirectory().
                        Append(chrome::kHelperProcessExecutablePath));

  InitMacCrashReporter(command_line, process_type);
#endif

  // Notice a user data directory override if any
  FilePath user_data_dir =
      command_line.GetSwitchValuePath(switches::kUserDataDir);
#if defined(OS_MACOSX) || defined(OS_WIN)
  CheckUserDataDirPolicy(&user_data_dir);
#endif
  if (!user_data_dir.empty()) {
    CHECK(PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_USER_DATA,
        user_data_dir,
        chrome::ProcessNeedsProfileDir(process_type)));
  }

  startup_timer_.reset(new base::StatsScope<base::StatsCounterTimer>
                       (content::Counters::chrome_main()));

  // Enable the heap profiler as early as possible!
  EnableHeapProfiler(command_line);

  // Enable Message Loop related state asap.
  if (command_line.HasSwitch(switches::kMessageLoopHistogrammer))
    MessageLoop::EnableHistogrammer(true);

  if (command_line.HasSwitch(switches::kSingleProcess))
    InitializeChromeContentRendererClient();

  logging::OldFileDeletionState file_state =
      logging::APPEND_TO_OLD_LOG_FILE;
  if (process_type.empty()) {
    file_state = logging::DELETE_OLD_LOG_FILE;
  }
  logging::InitChromeLogging(command_line, file_state);

#if defined(OS_WIN)
  // TODO(darin): Kill this once http://crbug.com/52609 is fixed.
  ui::SetResourcesDataDLL(_AtlBaseModule.GetResourceInstance());
#endif

  if (SubprocessNeedsResourceBundle(process_type)) {
    // Initialize ResourceBundle which handles files loaded from external
    // sources.  The language should have been passed in to us from the
    // browser process as a command line flag.
    DCHECK(command_line.HasSwitch(switches::kLang) ||
           process_type == switches::kZygoteProcess ||
           process_type == switches::kGpuProcess ||
           process_type == switches::kNaClLoaderProcess ||
           process_type == switches::kPpapiBrokerProcess ||
           process_type == switches::kPpapiPluginProcess);

    // TODO(markusheintz): The command line flag --lang is actually processed
    // by the CommandLinePrefStore, and made available through the PrefService
    // via the preference prefs::kApplicationLocale. The browser process uses
    // the --lang flag to pass the value of the PrefService in here. Maybe
    // this value could be passed in a different way.
    const std::string locale =
        command_line.GetSwitchValueASCII(switches::kLang);
    const std::string loaded_locale =
        ResourceBundle::InitSharedInstanceWithLocale(locale);
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " <<
        locale;

#if defined(OS_MACOSX)
    // Update the process name (need resources to get the strings, so
    // only do this when ResourcesBundle has been initialized).
    SetMacProcessName(command_line);
#endif  // defined(OS_MACOSX)
  }

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.  BrowserMain
  // sets this up for the browser process in a different manner. Zygotes
  // need to call InitCrashReporter() in RunZygote().
  if (!process_type.empty() && process_type != switches::kZygoteProcess)
    InitCrashReporter();
#endif

#if defined(OS_CHROMEOS)
  // Read and cache ChromeOS version from file,
  // to be used from inside the sandbox.
  int32 major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(
      &major_version, &minor_version, &bugfix_version);
#endif
}

void ChromeMainDelegate::SandboxInitialized(const std::string& process_type) {
  startup_timer_->Stop();  // End of Startup Time Measurement.

  // Note: If you are adding a new process type below, be sure to adjust the
  // AdjustLinuxOOMScore function too.
#if defined(OS_LINUX)
  AdjustLinuxOOMScore(process_type);
#endif
#if defined(OS_WIN)
  SuppressWindowsErrorDialogs();
#endif
}

int ChromeMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  static const MainFunction kMainFunctions[] = {
    { switches::kServiceProcess,     ServiceProcessMain },
#if defined(OS_MACOSX)
    { switches::kRelauncherProcess,
      mac_relauncher::internal::RelauncherMain },
#endif
#if !defined(DISABLE_NACL)
    { switches::kNaClLoaderProcess, NaClMain },
#endif  // DISABLE_NACL
  };

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_function_params);
  }

  return -1;
}

void ChromeMainDelegate::ProcessExiting(const std::string& process_type) {
  if (SubprocessNeedsResourceBundle(process_type))
    ResourceBundle::CleanupSharedInstance();

  logging::CleanupChromeLogging();
}

#if defined(OS_MACOSX)
bool ChromeMainDelegate::ProcessRegistersWithSystemProcess(
    const std::string& process_type) {
  return process_type == switches::kNaClLoaderProcess;
}

bool ChromeMainDelegate::ShouldSendMachPort(const std::string& process_type) {
  return process_type != switches::kRelauncherProcess &&
      process_type != switches::kServiceProcess;
}

bool ChromeMainDelegate::DelaySandboxInitialization(
    const std::string& process_type) {
  // NaClLoader does this in NaClMainPlatformDelegate::EnableSandbox().
  // No sandbox needed for relauncher.
  return process_type == switches::kNaClLoaderProcess ||
      process_type == switches::kRelauncherProcess;
}
#elif defined(OS_POSIX)
content::ZygoteForkDelegate* ChromeMainDelegate::ZygoteStarting() {
#if defined(DISABLE_NACL)
  return NULL;
#else
  return new NaClForkDelegate();
#endif
}

void ChromeMainDelegate::ZygoteForked() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  Profiling::ProcessStarted();
  if (Profiling::BeingProfiled()) {
    base::debug::RestartProfilingAfterFork();
    SetUpProfilingShutdownHandler();
  }

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.  BrowserMain sets
  // this up for the browser process in a different manner.
  InitCrashReporter();
#endif

  InitializeChromeContentClient(process_type);
}
#endif  // OS_MACOSX
