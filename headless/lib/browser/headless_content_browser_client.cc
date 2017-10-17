// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_content_browser_client.h"

#include <memory>
#include <unordered_set>

#include "base/base_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/lib/browser/headless_quota_permission_context.h"
#include "headless/lib/headless_macros.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"

#if defined(HEADLESS_USE_BREAKPAD)
#include "base/debug/leak_annotations.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif  // defined(HEADLESS_USE_BREAKPAD)

#if BUILDFLAG(ENABLE_BASIC_PRINTING) && !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "base/strings/utf_string_conversions.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#endif

namespace headless {

namespace {
const char kCapabilityPath[] =
    "interface_provider_specs.navigation:frame.provides.renderer";

#if defined(HEADLESS_USE_BREAKPAD)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type,
    const HeadlessBrowser::Options& options) {
  base::FilePath dumps_path = options.crash_dumps_dir;
  if (dumps_path.empty()) {
    bool ok = PathService::Get(base::DIR_MODULE, &dumps_path);
    DCHECK(ok);
  }

  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
#if defined(OFFICIAL_BUILD)
    // Upload crash dumps in official builds, unless we're running in unattended
    // mode (not to be confused with headless mode in general -- see
    // chrome/common/env_vars.cc).
    static const char kHeadless[] = "CHROME_HEADLESS";
    bool upload = (getenv(kHeadless) == nullptr);
#else
    bool upload = false;
#endif
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path, upload);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line,
                     const HeadlessBrowser::Options& options) {
  if (!breakpad::IsCrashReporterEnabled())
    return -1;

  std::string process_type =
      command_line.GetSwitchValueASCII(::switches::kProcessType);

  if (process_type == ::switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler =
        CreateCrashHandlerHost(process_type, options);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == ::switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler =
        CreateCrashHandlerHost(process_type, options);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == ::switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler =
        CreateCrashHandlerHost(process_type, options);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(HEADLESS_USE_BREAKPAD)

}  // namespace

HeadlessContentBrowserClient::HeadlessContentBrowserClient(
    HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessContentBrowserClient::~HeadlessContentBrowserClient() {}

content::BrowserMainParts* HeadlessContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams&) {
  std::unique_ptr<HeadlessBrowserMainParts> browser_main_parts =
      base::MakeUnique<HeadlessBrowserMainParts>(browser_);
  browser_->set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts.release();
}

void HeadlessContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  auto* browser_context = HeadlessBrowserContextImpl::From(
      render_view_host->GetProcess()->GetBrowserContext());
  const base::Callback<void(WebPreferences*)>& callback =
      browser_context->options()->override_web_preferences_callback();
  if (callback)
    callback.Run(prefs);
}

content::DevToolsManagerDelegate*
HeadlessContentBrowserClient::GetDevToolsManagerDelegate() {
  return new HeadlessDevToolsManagerDelegate(browser_->GetWeakPtr());
}

std::unique_ptr<base::Value>
HeadlessContentBrowserClient::GetServiceManifestOverlay(
    base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetBrowserServiceManifestOverlay();
  if (name == content::mojom::kRendererServiceName)
    return GetRendererServiceManifestOverlay();
  if (name == content::mojom::kPackagedServicesServiceName)
    return GetPackagedServicesServiceManifestOverlay();

  return nullptr;
}

void HeadlessContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  (*services)[printing::mojom::kServiceName] =
      base::ASCIIToUTF16("PDF Compositor Service");
#endif
}

std::unique_ptr<base::Value>
HeadlessContentBrowserClient::GetBrowserServiceManifestOverlay() {
  base::StringPiece manifest_template =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HEADLESS_BROWSER_MANIFEST_OVERLAY);
  std::unique_ptr<base::Value> manifest =
      base::JSONReader::Read(manifest_template);

  // Add mojo_service_names to renderer capability specified in options.
  base::DictionaryValue* manifest_dictionary = nullptr;
  CHECK(manifest->GetAsDictionary(&manifest_dictionary));

  base::ListValue* capability_list = nullptr;
  CHECK(manifest_dictionary->GetList(kCapabilityPath, &capability_list));

  for (std::string service_name : browser_->options()->mojo_service_names) {
    capability_list->AppendString(service_name);
  }

  return manifest;
}

std::unique_ptr<base::Value>
HeadlessContentBrowserClient::GetRendererServiceManifestOverlay() {
  base::StringPiece manifest_template =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HEADLESS_RENDERER_MANIFEST_OVERLAY);
  return base::JSONReader::Read(manifest_template);
}

std::unique_ptr<base::Value>
HeadlessContentBrowserClient::GetPackagedServicesServiceManifestOverlay() {
  base::StringPiece manifest_template =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HEADLESS_PACKAGED_SERVICES_MANIFEST_OVERLAY);
  return base::JSONReader::Read(manifest_template);
}

content::QuotaPermissionContext*
HeadlessContentBrowserClient::CreateQuotaPermissionContext() {
  return new HeadlessQuotaPermissionContext();
}

void HeadlessContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  storage::GetNominalDynamicSettings(
      partition->GetPath(), context->IsOffTheRecord(), std::move(callback));
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void HeadlessContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(HEADLESS_USE_BREAKPAD)
  int crash_signal_fd = GetCrashSignalFD(command_line, *browser_->options());
  if (crash_signal_fd >= 0)
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
#endif  // defined(HEADLESS_USE_BREAKPAD)
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

void HeadlessContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  command_line->AppendSwitch(::switches::kHeadless);
  const base::CommandLine& old_command_line(
      *base::CommandLine::ForCurrentProcess());
  if (old_command_line.HasSwitch(switches::kUserAgent)) {
    command_line->AppendSwitchNative(
        switches::kUserAgent,
        old_command_line.GetSwitchValueNative(switches::kUserAgent));
  }
#if defined(HEADLESS_USE_BREAKPAD)
  // This flag tells child processes to also turn on crash reporting.
  if (breakpad::IsCrashReporterEnabled())
    command_line->AppendSwitch(::switches::kEnableCrashReporter);
#endif  // defined(HEADLESS_USE_BREAKPAD)

  // If we're spawning a renderer, then override the language switch.
  if (command_line->GetSwitchValueASCII(::switches::kProcessType) ==
      ::switches::kRendererProcess) {
    content::RenderProcessHost* render_process_host =
        content::RenderProcessHost::FromID(child_process_id);
    if (render_process_host) {
      HeadlessBrowserContextImpl* headless_browser_context_impl =
          HeadlessBrowserContextImpl::From(
              render_process_host->GetBrowserContext());
      std::vector<base::StringPiece> languages = base::SplitStringPiece(
          headless_browser_context_impl->options()->accept_language(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (!languages.empty()) {
        command_line->AppendSwitchASCII(::switches::kLang,
                                        languages[0].as_string());
      }
    }
  }
}

void HeadlessContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    content::ResourceType resource_type,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  if (!callback.is_null()) {
    // If --allow-insecure-localhost is specified, and the request
    // was for localhost, then the error was not fatal.
    bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
        ::switches::kAllowInsecureLocalhost);
    if (allow_localhost && net::IsLocalhost(request_url.host())) {
      callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE);
      return;
    }

    callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
}

void HeadlessContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new HeadlessResourceDispatcherHostDelegate(
          browser_->options()->enable_resource_scheduler));
  content::ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

net::NetLog* HeadlessContentBrowserClient::GetNetLog() {
  return browser_->browser_main_parts()->net_log();
}

bool HeadlessContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  LockedPtr<HeadlessBrowserContextImpl> browser_context =
      browser_->GetBrowserContextForRenderFrame(render_process_id,
                                                render_frame_id);
  if (!browser_context)
    return false;
  return browser_context->options()->allow_cookies();
}

bool HeadlessContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id,
    const net::CookieOptions& options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  LockedPtr<HeadlessBrowserContextImpl> browser_context =
      browser_->GetBrowserContextForRenderFrame(render_process_id,
                                                render_frame_id);
  if (!browser_context)
    return false;
  return browser_context->options()->allow_cookies();
}

}  // namespace headless
