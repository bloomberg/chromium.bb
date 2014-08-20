// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_browser_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/shell/browser/ipc_echo_message_filter.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_devtools_delegate.h"
#include "content/shell/browser/shell_message_filter.h"
#include "content/shell/browser/shell_net_log.h"
#include "content/shell/browser/shell_notification_manager.h"
#include "content/shell/browser/shell_quota_permission_context.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/webkit_test_helpers.h"
#include "content/shell/geolocation/shell_access_token_store.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/path_service.h"
#include "components/breakpad/browser/crash_dump_manager_android.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "components/breakpad/app/breakpad_linux.h"
#include "components/breakpad/browser/crash_handler_host_linux.h"
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
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
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

int GetCrashSignalFD(const CommandLine& command_line) {
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

void RequestDesktopNotificationPermissionOnIO(
    const GURL& source_origin,
    RenderFrameHost* render_frame_host,
    const base::Callback<void(blink::WebNotificationPermission)>& callback) {
  ShellNotificationManager* manager =
      ShellContentBrowserClient::Get()->GetShellNotificationManager();
  if (manager)
    manager->RequestPermission(source_origin, callback);
  else
    callback.Run(blink::WebNotificationPermissionAllowed);
}

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
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  webkit_source_dir_ = GetWebKitRootDirFilePath();
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  g_browser_client = NULL;
}

ShellNotificationManager*
ShellContentBrowserClient::GetShellNotificationManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return NULL;

  if (!shell_notification_manager_)
    shell_notification_manager_.reset(new ShellNotificationManager());

  return shell_notification_manager_.get();
}

BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  shell_browser_main_parts_ = new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

void ShellContentBrowserClient::RenderProcessWillLaunch(
    RenderProcessHost* host) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kExposeIpcEcho))
    host->AddFilter(new IPCEchoMessageFilter());
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  host->AddFilter(new ShellMessageFilter(
      host->GetID(),
      BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetDatabaseTracker(),
      BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetQuotaManager(),
      BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetURLRequestContext()));
  host->Send(new ShellViewMsg_SetWebKitSourceDir(webkit_source_dir_));
}

net::URLRequestContextGetter* ShellContentBrowserClient::CreateRequestContext(
    BrowserContext* content_browser_context,
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors) {
  ShellBrowserContext* shell_browser_context =
      ShellBrowserContextForBrowserContext(content_browser_context);
  return shell_browser_context->CreateRequestContext(
      protocol_handlers, request_interceptors.Pass());
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
      partition_path,
      in_memory,
      protocol_handlers,
      request_interceptors.Pass());
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  DCHECK_EQ(url.scheme(), base::StringToLowerASCII(url.scheme()));
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

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    command_line->AppendSwitch(switches::kDumpRenderTree);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableFontAntialiasing))
    command_line->AppendSwitch(switches::kEnableFontAntialiasing);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExposeInternalsForTesting))
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExposeIpcEcho))
    command_line->AppendSwitch(switches::kExposeIpcEcho);
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kStableReleaseMode))
    command_line->AppendSwitch(switches::kStableReleaseMode);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    command_line->AppendSwitch(switches::kEnableCrashReporter);
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kCrashDumpsDir)) {
    command_line->AppendSwitchPath(
        switches::kCrashDumpsDir,
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kCrashDumpsDir));
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    command_line->AppendSwitchASCII(
        switches::kEnableLeakDetection,
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection));
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kRegisterFontFiles)) {
    command_line->AppendSwitchASCII(
        switches::kRegisterFontFiles,
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kRegisterFontFiles));
  }
}

void ShellContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    const GURL& url,
    WebPreferences* prefs) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  WebKitTestController::Get()->OverrideWebkitPrefs(prefs);
}

void ShellContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new ShellResourceDispatcherHostDelegate());
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

void ShellContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    RenderFrameHost* render_frame_host,
    const base::Callback<void(blink::WebNotificationPermission)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&RequestDesktopNotificationPermissionOnIO,
                              source_origin,
                              render_frame_host,
                              callback));
}

blink::WebNotificationPermission
ShellContentBrowserClient::CheckDesktopNotificationPermission(
    const GURL& source_url,
    ResourceContext* context,
    int render_process_id) {
  ShellNotificationManager* manager = GetShellNotificationManager();
  if (manager)
    return manager->CheckPermission(source_url);

  return blink::WebNotificationPermissionAllowed;
}

SpeechRecognitionManagerDelegate*
    ShellContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
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

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const CommandLine& command_line,
    int child_process_id,
    std::vector<FileDescriptorInfo>* mappings) {
#if defined(OS_ANDROID)
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::FilePath pak_file;
  bool r = PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_file);
  CHECK(r);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("paks"));
  pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));

  base::File f(pak_file, flags);
  if (!f.IsValid()) {
    NOTREACHED() << "Failed to open file when creating renderer process: "
                 << "content_shell.pak";
  }
  mappings->push_back(
      FileDescriptorInfo(kShellPakDescriptor, base::FileDescriptor(f.Pass())));

  if (breakpad::IsCrashReporterEnabled()) {
    f = breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFile(
        child_process_id);
    if (!f.IsValid()) {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 << "be disabled for this process.";
    } else {
      mappings->push_back(
          FileDescriptorInfo(kAndroidMinidumpDescriptor,
                             base::FileDescriptor(f.Pass())));
    }
  }
#else  // !defined(OS_ANDROID)
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->push_back(FileDescriptorInfo(
        kCrashDumpSignal, base::FileDescriptor(crash_signal_fd, false)));
  }
#endif  // defined(OS_ANDROID)
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_WIN)
void ShellContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                                 bool* success) {
  // Add sideloaded font files for testing. See also DIR_WINDOWS_FONTS
  // addition in |StartSandboxedProcess|.
  std::vector<std::string> font_files = GetSideloadFontFiles();
  for (std::vector<std::string>::const_iterator i(font_files.begin());
      i != font_files.end();
      ++i) {
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
        sandbox::TargetPolicy::FILES_ALLOW_READONLY,
        base::UTF8ToWide(*i).c_str());
  }
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
