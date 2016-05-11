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
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/layouttest_support.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/shell/app/blink_test_platform_support.h"
#include "content/shell/app/shell_crash_reporter_client.h"
#include "content/shell/browser/layout_test/layout_test_browser_main.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/shell_browser_main.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/layout_test/layout_test_content_client.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_content_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/layout_test_content_renderer_client.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "content/shell/utility/shell_content_utility_client.h"
#include "media/base/media_switches.h"
#include "media/base/mime_util.h"
#include "net/cookies/cookie_monster.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_switches.h"
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
#include "base/android/apk_assets.h"
#include "base/posix/global_descriptors.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/os_crash_dumps.h"
#include "components/crash/content/app/breakpad_mac.h"
#include "content/shell/app/paths_mac.h"
#include "content/shell/app/shell_main_delegate_mac.h"
#endif  // OS_MACOSX

#if defined(OS_WIN)
#include <windows.h>
#include <initguid.h>
#include "base/logging_win.h"
#include "components/crash/content/app/breakpad_win.h"
#include "content/shell/common/v8_breakpad_support_win.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "components/crash/content/app/breakpad_linux.h"
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
  int dummy;
  if (!exit_code)
    exit_code = &dummy;

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
      *exit_code = 1;
      return true;
    }
  }

  if (command_line.HasSwitch(switches::kRunLayoutTest)) {
    EnableBrowserLayoutTestMode();

#if defined(ENABLE_PLUGINS)
    if (!ppapi::RegisterBlinkTestPlugin(&command_line)) {
      *exit_code = 1;
      return true;
    }
#endif
    command_line.AppendSwitch(switches::kProcessPerTab);
    command_line.AppendSwitch(switches::kEnableLogging);
    command_line.AppendSwitch(switches::kAllowFileAccessFromFiles);
    // only default to osmesa if the flag isn't already specified.
    if (!command_line.HasSwitch(switches::kUseGL)) {
      command_line.AppendSwitchASCII(switches::kUseGL,
                                     gfx::kGLImplementationOSMesaName);
    }
    command_line.AppendSwitch(switches::kSkipGpuDataLoading);
    command_line.AppendSwitchASCII(switches::kTouchEvents,
                                   switches::kTouchEventsEnabled);
    if (!command_line.HasSwitch(switches::kForceDeviceScaleFactor))
      command_line.AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
    command_line.AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);

    if (!command_line.HasSwitch(switches::kStableReleaseMode)) {
      command_line.AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
      // Only enable WebBluetooth during Layout Tests in non release mode.
      command_line.AppendSwitch(switches::kEnableWebBluetooth);
    }

    if (!command_line.HasSwitch(switches::kEnableThreadedCompositing)) {
      command_line.AppendSwitch(switches::kDisableThreadedCompositing);
      command_line.AppendSwitch(cc::switches::kDisableThreadedAnimation);
    }

    if (!command_line.HasSwitch(switches::kEnableDisplayList2dCanvas)) {
      command_line.AppendSwitch(switches::kDisableDisplayList2dCanvas);
    }

    command_line.AppendSwitch(switches::kEnableInbandTextTracks);
    command_line.AppendSwitch(switches::kMuteAudio);

    command_line.AppendSwitch(switches::kEnablePreciseMemoryInfo);

    command_line.AppendSwitchASCII(switches::kHostResolverRules,
                                   "MAP *.test 127.0.0.1");

    command_line.AppendSwitch(switches::kEnablePartialRaster);

    if (!command_line.HasSwitch(switches::kForceGpuRasterization) &&
        !command_line.HasSwitch(switches::kEnableGpuRasterization)) {
      command_line.AppendSwitch(switches::kDisableGpuRasterization);
    }

    // Unless/until WebM files are added to the media layout tests, we need to
    // avoid removing MP4/H264/AAC so that layout tests can run on Android.
#if !defined(OS_ANDROID)
    media::RemoveProprietaryMediaTypesAndCodecsForTests();
#endif

    if (!BlinkTestPlatformInitialize()) {
      *exit_code = 1;
      return true;
    }
  }

  content_client_.reset(base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kRunLayoutTest)
                            ? new LayoutTestContentClient
                            : new ShellContentClient);
  SetContentClient(content_client_.get());

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
  std::unique_ptr<BrowserMainRunner> browser_runner_;
#endif

  base::trace_event::TraceLog::GetInstance()->SetProcessName("Browser");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

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
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->MaybeGet(kShellPakDescriptor);
  base::MemoryMappedFile::Region pak_region;
  if (pak_fd >= 0) {
    pak_region = global_descriptors->GetRegion(kShellPakDescriptor);
  } else {
    pak_fd =
        base::android::OpenApkAsset("assets/content_shell.pak", &pak_region);
    // Loaded from disk for browsertests.
    if (pak_fd < 0) {
      base::FilePath pak_file;
      bool r = PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_file);
      DCHECK(r);
      pak_file = pak_file.Append(FILE_PATH_LITERAL("paks"));
      pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));
      int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      pak_fd = base::File(pak_file, flags).TakePlatformFile();
      pak_region = base::MemoryMappedFile::Region::kWholeFile;
    }
    global_descriptors->Set(kShellPakDescriptor, pak_fd, pak_region);
  }
  DCHECK_GE(pak_fd, 0);
  // This is clearly wrong. See crbug.com/330930
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                          pak_region);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(pak_fd), pak_region, ui::SCALE_FACTOR_100P);
#else  // defined(OS_ANDROID)
#if defined(OS_MACOSX)
  base::FilePath pak_file = GetResourcesPakFilePath();
#else
  base::FilePath pak_file;
  bool r = PathService::Get(base::DIR_MODULE, &pak_file);
  DCHECK(r);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));
#endif  // defined(OS_MACOSX)
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
#endif  // defined(OS_ANDROID)
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

ContentUtilityClient* ShellMainDelegate::CreateContentUtilityClient() {
  utility_client_.reset(new ShellContentUtilityClient);
  return utility_client_.get();
}

}  // namespace content
