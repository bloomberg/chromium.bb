// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_main_delegate.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cc/base/switches.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/app/blink_test_platform_support.h"
#include "content/shell/app/shell_crash_reporter_client.h"
#include "content/shell/browser/layout_test/layout_test_browser_main.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/shell_browser_main.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/layout_test_content_renderer_client.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "media/base/media_switches.h"
#include "net/cookies/cookie_monster.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.

#if defined(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
    content::RegisterIPCLogger(msg_id, logger)
#include "content/shell/common/shell_messages.h"
#endif

#if defined(OS_ANDROID)
#include "base/posix/global_descriptors.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/os_crash_dumps.h"
#include "components/crash/app/breakpad_mac.h"
#include "content/shell/app/paths_mac.h"
#include "content/shell/app/shell_main_delegate_mac.h"
#endif  // OS_MACOSX

#if defined(OS_WIN)
#include <initguid.h>
#include <windows.h>
#include "base/logging_win.h"
#include "components/crash/app/breakpad_win.h"
#include "content/shell/common/v8_breakpad_support_win.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "components/crash/app/breakpad_linux.h"
#endif

namespace {

base::LazyInstance<content::ShellCrashReporterClient>::Leaky
    g_shell_crash_client = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_WIN)
// If "Content Shell" doesn't show up in your list of trace providers in
// Sawbuck, add these registry entries to your machine (NOTE the optional
// Wow6432Node key for x64 machines):
// 1. Find:  HKLM\SOFTWARE\[Wow6432Node\]Google\Sawbuck\Providers
// 2. Add a subkey with the name "{6A3E50A4-7E15-4099-8413-EC94D8C2A4B6}"
// 3. Add these values:
//    "default_flags"=dword:00000001
//    "default_level"=dword:00000004
//    @="Content Shell"

// {6A3E50A4-7E15-4099-8413-EC94D8C2A4B6}
const GUID kContentShellProviderName = {
    0x6a3e50a4, 0x7e15, 0x4099,
        { 0x84, 0x13, 0xec, 0x94, 0xd8, 0xc2, 0xa4, 0xb6 } };
#endif

void InitLogging() {
  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("content_shell.log");
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true, true, true, true);
}

}  // namespace

namespace content {

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  // "dump-render-tree" has been renamed to "run-layout-test", but the old
  // flag name is still used in some places, so this check will remain until
  // it is phased out entirely.
  if (command_line.HasSwitch(switches::kDumpRenderTree))
    command_line.AppendSwitch(switches::kRunLayoutTest);

#if defined(OS_WIN)
  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kContentShellProviderName);

  v8_breakpad_support::SetUp();
#endif
#if defined(OS_MACOSX)
  // Needs to happen before InitializeResourceBundle() and before
  // BlinkTestPlatformInitialize() are called.
  OverrideFrameworkBundlePath();
  OverrideChildProcessPath();
  EnsureCorrectResolutionSettings();
#endif  // OS_MACOSX

  InitLogging();
  if (command_line.HasSwitch(switches::kCheckLayoutTestSysDeps)) {
    // If CheckLayoutSystemDeps succeeds, we don't exit early. Instead we
    // continue and try to load the fonts in BlinkTestPlatformInitialize
    // below, and then try to bring up the rest of the content module.
    if (!CheckLayoutSystemDeps()) {
      if (exit_code)
        *exit_code = 1;
      return true;
    }
  }

  if (command_line.HasSwitch(switches::kRunLayoutTest)) {
    EnableBrowserLayoutTestMode();

    command_line.AppendSwitch(switches::kProcessPerTab);
    command_line.AppendSwitch(switches::kEnableLogging);
    command_line.AppendSwitch(switches::kAllowFileAccessFromFiles);
    command_line.AppendSwitchASCII(switches::kUseGL,
                                   gfx::kGLImplementationOSMesaName);
    command_line.AppendSwitch(switches::kSkipGpuDataLoading);
    command_line.AppendSwitchASCII(switches::kTouchEvents,
                                   switches::kTouchEventsEnabled);
    command_line.AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
#if defined(OS_ANDROID)
    command_line.AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
#endif

    if (!command_line.HasSwitch(switches::kStableReleaseMode)) {
      command_line.AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    }

    if (!command_line.HasSwitch(switches::kEnableThreadedCompositing)) {
      command_line.AppendSwitch(switches::kDisableThreadedCompositing);
      command_line.AppendSwitch(cc::switches::kDisableThreadedAnimation);
      // Text blobs are normally disabled when kDisableImplSidePainting is
      // present to ensure correct LCD behavior, but for layout tests we want
      // them on because LCD is always suppressed.
      command_line.AppendSwitch(switches::kForceTextBlobs);
    }

    if (!command_line.HasSwitch(switches::kEnableDisplayList2dCanvas)) {
      command_line.AppendSwitch(switches::kDisableDisplayList2dCanvas);
    }

    command_line.AppendSwitch(switches::kEnableInbandTextTracks);
    command_line.AppendSwitch(switches::kMuteAudio);

    // TODO: crbug.com/311404 Make layout tests work w/ delegated renderer.
    command_line.AppendSwitch(switches::kDisableDelegatedRenderer);
    command_line.AppendSwitch(cc::switches::kCompositeToMailbox);

    command_line.AppendSwitch(switches::kEnableFileCookies);

    command_line.AppendSwitch(switches::kEnablePreciseMemoryInfo);

    command_line.AppendSwitchASCII(switches::kHostResolverRules,
                                   "MAP *.test 127.0.0.1");

    // TODO(wfh): crbug.com/295137 Remove this when NPAPI is gone.
    command_line.AppendSwitch(switches::kEnableNpapi);

    // Unless/until WebM files are added to the media layout tests, we need to
    // avoid removing MP4/H264/AAC so that layout tests can run on Android.
#if !defined(OS_ANDROID)
    net::RemoveProprietaryMediaTypesAndCodecsForTests();
#endif

    if (!BlinkTestPlatformInitialize()) {
      if (exit_code)
        *exit_code = 1;
      return true;
    }
  }
  SetContentClient(&content_client_);
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    crash_reporter::SetCrashReporterClient(g_shell_crash_client.Pointer());
#if defined(OS_MACOSX)
    base::mac::DisableOSCrashDumps();
    breakpad::InitCrashReporter(process_type);
    breakpad::InitCrashProcessInfo(process_type);
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
    if (process_type != switches::kZygoteProcess) {
#if defined(OS_ANDROID)
      if (process_type.empty())
        breakpad::InitCrashReporter(process_type);
      else
        breakpad::InitNonBrowserCrashReporterForAndroid(process_type);
#else
      breakpad::InitCrashReporter(process_type);
#endif
    }
#elif defined(OS_WIN)
    UINT new_flags =
        SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);
    breakpad::InitCrashReporter(process_type);
#endif
  }

  InitializeResourceBundle();
}

int ShellMainDelegate::RunProcess(
    const std::string& process_type,
    const MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

#if !defined(OS_ANDROID)
  // Android stores the BrowserMainRunner instance as a scoped member pointer
  // on the ShellMainDelegate class because of different object lifetime.
  scoped_ptr<BrowserMainRunner> browser_runner_;
#endif

  browser_runner_.reset(BrowserMainRunner::Create());
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kRunLayoutTest) ||
                 command_line.HasSwitch(switches::kCheckLayoutTestSysDeps)
             ? LayoutTestBrowserMain(main_function_params, browser_runner_)
             : ShellBrowserMain(main_function_params, browser_runner_);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
void ShellMainDelegate::ZygoteForked() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    breakpad::InitCrashReporter(process_type);
  }
}
#endif

void ShellMainDelegate::InitializeResourceBundle() {
#if defined(OS_ANDROID)
  // In the Android case, the renderer runs with a different UID and can never
  // access the file system.  So we are passed a file descriptor to the
  // ResourceBundle pak at launch time.
  int pak_fd =
      base::GlobalDescriptors::GetInstance()->MaybeGet(kShellPakDescriptor);
  if (pak_fd >= 0) {
    // This is clearly wrong. See crbug.com/330930
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        base::File(pak_fd), base::MemoryMappedFile::Region::kWholeFile);
    ResourceBundle::GetSharedInstance().AddDataPackFromFile(
        base::File(pak_fd), ui::SCALE_FACTOR_100P);
    return;
  }
#endif

  base::FilePath pak_file;
#if defined(OS_MACOSX)
  pak_file = GetResourcesPakFilePath();
#else
  base::FilePath pak_dir;

#if defined(OS_ANDROID)
  bool got_path = PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_dir);
  DCHECK(got_path);
  pak_dir = pak_dir.Append(FILE_PATH_LITERAL("paks"));
#else
  PathService::Get(base::DIR_MODULE, &pak_dir);
#endif

  pak_file = pak_dir.Append(FILE_PATH_LITERAL("content_shell.pak"));
#endif
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kRunLayoutTest)
                            ? new LayoutTestContentBrowserClient
                            : new ShellContentBrowserClient);

  return browser_client_.get();
}

ContentRendererClient* ShellMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(base::CommandLine::ForCurrentProcess()->HasSwitch(
                             switches::kRunLayoutTest)
                             ? new LayoutTestContentRendererClient
                             : new ShellContentRendererClient);

  return renderer_client_.get();
}

}  // namespace content
