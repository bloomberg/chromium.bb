// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/cast_main_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/common/cast_paths.h"
#include "chromecast/common/cast_resource_delegate.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/crash/cast_crash_reporter_client.h"
#include "chromecast/renderer/cast_content_renderer_client.h"
#include "components/crash/app/crash_reporter_client.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "chromecast/crash/android/crash_handler.h"
#endif  // defined(OS_ANDROID)

namespace {

#if !defined(OS_ANDROID)
base::LazyInstance<chromecast::CastCrashReporterClient>::Leaky
    g_crash_reporter_client = LAZY_INSTANCE_INITIALIZER;
#endif  // !defined(OS_ANDROID)

}  // namespace

namespace chromecast {
namespace shell {

CastMainDelegate::CastMainDelegate() {
}

CastMainDelegate::~CastMainDelegate() {
}

bool CastMainDelegate::BasicStartupComplete(int* exit_code) {
  RegisterPathProvider();

  logging::LoggingSettings settings;
#if defined(OS_ANDROID)
  base::FilePath log_file;
  PathService::Get(FILE_CAST_ANDROID_LOG, &log_file);
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_file.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
#else
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
#endif  // defined(OS_ANDROID)
  logging::InitLogging(settings);
  // Time, process, and thread ID are available through logcat.
  logging::SetLogItems(true, true, false, false);

  content::SetContentClient(&content_client_);
  return false;
}

void CastMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache the
  // results. This data needs to be cached when file-reading is still allowed,
  // since base::CPU expects to be callable later, when file-reading is no
  // longer allowed.
  base::CPU cpu_info;
#endif

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_ANDROID)
  base::FilePath log_file;
  PathService::Get(FILE_CAST_ANDROID_LOG, &log_file);
  chromecast::CrashHandler::Initialize(process_type, log_file);
#else
  crash_reporter::SetCrashReporterClient(g_crash_reporter_client.Pointer());

  if (process_type != switches::kZygoteProcess) {
    CastCrashReporterClient::InitCrashReporter(process_type);
  }
#endif  // defined(OS_ANDROID)

  InitializeResourceBundle();
}

int CastMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
#if defined(OS_ANDROID)
  if (!process_type.empty())
    return -1;

  // Note: Android must handle running its own browser process.
  // See ChromeMainDelegateAndroid::RunProcess.
  browser_runner_.reset(content::BrowserMainRunner::Create());
  return browser_runner_->Initialize(main_function_params);
#else
  return -1;
#endif  // defined(OS_ANDROID)
}

#if !defined(OS_ANDROID)
void CastMainDelegate::ZygoteForked() {
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  CastCrashReporterClient::InitCrashReporter(process_type);
}
#endif  // !defined(OS_ANDROID)

void CastMainDelegate::InitializeResourceBundle() {
#if defined(OS_ANDROID)
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.
  int pak_fd =
      base::GlobalDescriptors::GetInstance()->MaybeGet(kAndroidPakDescriptor);
  if (pak_fd >= 0) {
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        base::File(pak_fd), base::MemoryMappedFile::Region::kWholeFile);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
        base::File(pak_fd), ui::SCALE_FACTOR_100P);
    return;
  }
#endif  // defined(OS_ANDROID)

  resource_delegate_.reset(new CastResourceDelegate());
  // TODO(gunsch): Use LOAD_COMMON_RESOURCES once ResourceBundle no longer
  // hardcodes resource file names.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US",
      resource_delegate_.get(),
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

  base::FilePath pak_file;
  CHECK(PathService::Get(FILE_CAST_PAK, &pak_file));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file,
      ui::SCALE_FACTOR_NONE);
}

content::ContentBrowserClient* CastMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new CastContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient*
CastMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(new CastContentRendererClient);
  return renderer_client_.get();
}

}  // namespace shell
}  // namespace chromecast
