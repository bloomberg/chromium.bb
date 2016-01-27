// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_browser_client.h"

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/test_mojo_app.h"
#include "content/shell/browser/blink_test_controller.h"
#include "content/shell/browser/layout_test/layout_test_browser_main_parts.h"
#include "content/shell/browser/layout_test/layout_test_resource_dispatcher_host_delegate.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_access_token_store.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/shell_net_log.h"
#include "content/shell/browser/shell_quota_permission_context.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/sandbox.h"
#endif

namespace content {

namespace {

ShellContentBrowserClient* g_browser_client;
bool g_swap_processes_for_redirect = false;

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCrashDumpsDir);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(
            process_type, dumps_path, false);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (!breakpad::IsCrashReporterEnabled())
    return -1;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)

}  // namespace

ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  return g_browser_client;
}

void ShellContentBrowserClient::SetSwapProcessesForRedirect(bool swap) {
  g_swap_processes_for_redirect = swap;
}

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(NULL) {
  DCHECK(!g_browser_client);
  g_browser_client = this;
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  g_browser_client = NULL;
}

BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  shell_browser_main_parts_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kRunLayoutTest)
                                  ? new LayoutTestBrowserMainParts(parameters)
                                  : new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

bool ShellContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_url) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch(switches::kIsolateSitesForTesting));
  std::string pattern =
      command_line->GetSwitchValueASCII(switches::kIsolateSitesForTesting);
  // Practically |origin| is the same as |effective_url.spec()|, except Origin
  // serialization strips the trailing "/", which makes for cleaner patterns.
  std::string origin = url::Origin(effective_url).Serialize();
  return base::MatchPattern(origin, pattern);
}

net::URLRequestContextGetter* ShellContentBrowserClient::CreateRequestContext(
    BrowserContext* content_browser_context,
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors) {
  ShellBrowserContext* shell_browser_context =
      ShellBrowserContextForBrowserContext(content_browser_context);
  return shell_browser_context->CreateRequestContext(
      protocol_handlers, std::move(request_interceptors));
}

net::URLRequestContextGetter*
ShellContentBrowserClient::CreateRequestContextForStoragePartition(
    BrowserContext* content_browser_context,
    const base::FilePath& partition_path,
    bool in_memory,
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors) {
  ShellBrowserContext* shell_browser_context =
      ShellBrowserContextForBrowserContext(content_browser_context);
  return shell_browser_context->CreateRequestContextForStoragePartition(
      partition_path, in_memory, protocol_handlers,
      std::move(request_interceptors));
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  // Keep in sync with ProtocolHandlers added by
  // ShellURLRequestContextGetter::GetURLRequestContext().
  static const char* const kProtocolList[] = {
      url::kBlobScheme,
      url::kFileSystemScheme,
      kChromeUIScheme,
      kChromeDevToolsScheme,
      url::kDataScheme,
      url::kFileScheme,
  };
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (url.scheme() == kProtocolList[i])
      return true;
  }
  return false;
}

bool ShellContentBrowserClient::IsNPAPIEnabled() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

void ShellContentBrowserClient::RegisterInProcessMojoApplications(
    StaticMojoApplicationMap* apps) {
#if (ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  apps->insert(std::make_pair(
      GURL("mojo:media"), base::Bind(&media::MojoMediaApplication::CreateApp)));
#endif
}

void ShellContentBrowserClient::RegisterOutOfProcessMojoApplications(
      OutOfProcessMojoApplicationMap* apps) {
  apps->insert(std::make_pair(GURL(kTestMojoAppUrl),
                              base::UTF8ToUTF16("Test Mojo App")));
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRunLayoutTest)) {
    command_line->AppendSwitch(switches::kRunLayoutTest);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpLineBoxTrees)) {
    command_line->AppendSwitch(switches::kDumpLineBoxTrees);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing)) {
    command_line->AppendSwitch(switches::kEnableFontAntialiasing);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlwaysUseComplexText)) {
    command_line->AppendSwitch(switches::kAlwaysUseComplexText);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    command_line->AppendSwitch(switches::kStableReleaseMode);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    command_line->AppendSwitch(switches::kEnableCrashReporter);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir)) {
    command_line->AppendSwitchPath(
        switches::kCrashDumpsDir,
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kCrashDumpsDir));
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    command_line->AppendSwitchASCII(
        switches::kEnableLeakDetection,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection));
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIsolateSitesForTesting)) {
    command_line->AppendSwitchASCII(
        switches::kIsolateSitesForTesting,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kIsolateSitesForTesting));
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRegisterFontFiles)) {
    command_line->AppendSwitchASCII(
        switches::kRegisterFontFiles,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kRegisterFontFiles));
  }
}

void ShellContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    WebPreferences* prefs) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRunLayoutTest))
    return;
  BlinkTestController::Get()->OverrideWebkitPrefs(prefs);
}

void ShellContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRunLayoutTest)
          ? new LayoutTestResourceDispatcherHostDelegate
          : new ShellResourceDispatcherHostDelegate);
  ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

WebContentsViewDelegate* ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
#if !defined(USE_AURA)
  return CreateShellWebContentsViewDelegate(web_contents);
#else
  return NULL;
#endif
}

QuotaPermissionContext*
ShellContentBrowserClient::CreateQuotaPermissionContext() {
  return new ShellQuotaPermissionContext();
}

void ShellContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    scoped_ptr<ClientCertificateDelegate> delegate) {
  if (!select_client_certificate_callback_.is_null())
    select_client_certificate_callback_.Run();
}

SpeechRecognitionManagerDelegate*
    ShellContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new ShellSpeechRecognitionManagerDelegate();
}

net::NetLog* ShellContentBrowserClient::GetNetLog() {
  return shell_browser_main_parts_->net_log();
}

bool ShellContentBrowserClient::ShouldSwapProcessesForRedirect(
    ResourceContext* resource_context,
    const GURL& current_url,
    const GURL& new_url) {
  return g_swap_processes_for_redirect;
}

DevToolsManagerDelegate*
ShellContentBrowserClient::GetDevToolsManagerDelegate() {
  return new ShellDevToolsManagerDelegate();
}

void ShellContentBrowserClient::OpenURL(
    BrowserContext* browser_context,
    const OpenURLParams& params,
    const base::Callback<void(WebContents*)>& callback) {
  callback.Run(Shell::CreateNewWindow(browser_context,
                                      params.url,
                                      nullptr,
                                      gfx::Size())->web_contents());
}

#if defined(OS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings,
    std::map<int, base::MemoryMappedFile::Region>* regions) {
  mappings->Share(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor));
  regions->insert(std::make_pair(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor)));

  if (breakpad::IsCrashReporterEnabled()) {
    base::File f(breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFile(
        child_process_id));
    if (!f.IsValid()) {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 << "be disabled for this process.";
    } else {
      mappings->Transfer(kAndroidMinidumpDescriptor,
                         base::ScopedFD(f.TakePlatformFile()));
    }
  }
}
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
bool ShellContentBrowserClient::PreSpawnRenderer(
    sandbox::TargetPolicy* policy) {
  // Add sideloaded font files for testing. See also DIR_WINDOWS_FONTS
  // addition in |StartSandboxedProcess|.
  std::vector<std::string> font_files = switches::GetSideloadFontFiles();
  for (std::vector<std::string>::const_iterator i(font_files.begin());
      i != font_files.end();
      ++i) {
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
        sandbox::TargetPolicy::FILES_ALLOW_READONLY,
        base::UTF8ToWide(*i).c_str());
  }
  return true;
}
#endif  // OS_WIN

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

ShellBrowserContext*
    ShellContentBrowserClient::off_the_record_browser_context() {
  return shell_browser_main_parts_->off_the_record_browser_context();
}

AccessTokenStore* ShellContentBrowserClient::CreateAccessTokenStore() {
  return new ShellAccessTokenStore(browser_context());
}

ShellBrowserContext*
ShellContentBrowserClient::ShellBrowserContextForBrowserContext(
    BrowserContext* content_browser_context) {
  if (content_browser_context == browser_context())
    return browser_context();
  DCHECK_EQ(content_browser_context, off_the_record_browser_context());
  return off_the_record_browser_context();
}

}  // namespace content
