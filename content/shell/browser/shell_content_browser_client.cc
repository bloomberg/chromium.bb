// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_browser_client.h"

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/test_service.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/shell_login_dialog.h"
#include "content/shell/browser/shell_net_log.h"
#include "content/shell/browser/shell_quota_permission_context.h"
#include "content/shell/browser/shell_url_request_context_getter.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/power_monitor_test.mojom.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test.mojom.h"
#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom.h"
#include "content/shell/common/web_test/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"
#include "media/mojo/buildflags.h"
#include "net/ssl/client_cert_identity.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/test/echo/manifest.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "storage/browser/quota/quota_settings.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "components/crash/content/app/crashpad.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_CHROMEOS)
// TODO(https://crbug.com/784179): Remove nogncheck.
#include "content/public/browser/context_factory.h"
#include "content/public/browser/gpu_interface_provider_factory.h"
#include "services/ws/public/mojom/constants.mojom.h"         // nogncheck
#include "services/ws/test_ws/manifest.h"                     // nogncheck
#include "services/ws/test_ws/test_window_service_factory.h"  // nogncheck
#include "services/ws/test_ws/test_ws.mojom.h"                // nogncheck
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "base/debug/leak_annotations.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "media/mojo/interfaces/constants.mojom.h"      // nogncheck
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

namespace content {

namespace {

ShellContentBrowserClient* g_browser_client;

#if defined(OS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif defined(OS_LINUX)
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
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_ANDROID)

const service_manager::Manifest& GetContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability(
            "renderer",
            service_manager::Manifest::InterfaceList<
                mojom::MojoWebTestHelper, mojom::FakeBluetoothChooser,
                mojom::WebTestBluetoothFakeAdapterSetter,
                bluetooth::mojom::FakeBluetooth>())
        .RequireCapability(echo::mojom::kServiceName, "echo")
#if defined(OS_CHROMEOS)
        .RequireCapability(ws::mojom::kServiceName, "test")
        .RequireCapability("test_ws", "test")
#endif
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "renderer",
            service_manager::Manifest::InterfaceList<
                mojom::MojoWebTestHelper>())
        .Build()
  };
  return *manifest;
}

const service_manager::Manifest& GetContentGpuOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability("browser", service_manager::Manifest::InterfaceList<
                                           mojom::PowerMonitorTest>())
          .Build()};
  return *manifest;
}

const service_manager::Manifest& GetContentPackagedServicesOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .PackageService(echo::GetManifest())
#if defined(OS_CHROMEOS)
        .PackageService(test_ws::GetManifest())
#endif
        .Build()
  };
  return *manifest;
}

const service_manager::Manifest& GetContentRendererOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability(
              "browser",
              service_manager::Manifest::InterfaceList<mojom::PowerMonitorTest,
                                                       mojom::TestService>())
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "browser",
              service_manager::Manifest::InterfaceList<mojom::WebTestControl>())
          .Build()};
  return *manifest;
}

const service_manager::Manifest& GetContentUtilityOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability(
              "browser",
              service_manager::Manifest::InterfaceList<mojom::PowerMonitorTest,
                                                       mojom::TestService>())
          .Build()};
  return *manifest;
}

}  // namespace

std::string GetShellUserAgent() {
  std::string product = "Chrome/" CONTENT_SHELL_VERSION;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
  return BuildUserAgentFromProduct(product);
}

ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  return g_browser_client;
}

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(nullptr) {
  DCHECK(!g_browser_client);
  g_browser_client = this;
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  g_browser_client = nullptr;
}

BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  shell_browser_main_parts_ = new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
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
  for (size_t i = 0; i < base::size(kProtocolList); ++i) {
    if (url.scheme() == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(url.scheme());
}

void ShellContentBrowserClient::BindInterfaceRequestFromFrame(
    RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!frame_interfaces_) {
    frame_interfaces_ = std::make_unique<
        service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>>();
    ExposeInterfacesToFrame(frame_interfaces_.get());
  }

  frame_interfaces_->TryBindInterface(interface_name, &interface_pipe,
                                      render_frame_host);
}

void ShellContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
  (*services)[kTestServiceUrl] =
      base::BindRepeating(&base::ASCIIToUTF16, "Test Service");
  (*services)[echo::mojom::kServiceName] =
      base::BindRepeating(&base::ASCIIToUTF16, "Echo Service");
#if defined(OS_CHROMEOS)
  if (features::IsMultiProcessMash()) {
    (*services)[test_ws::mojom::kServiceName] =
        base::BindRepeating(&base::ASCIIToUTF16, "Test Window Service");
  }
#endif
}

void ShellContentBrowserClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  if (service_name == media::mojom::kMediaServiceName) {
    service_manager::Service::RunAsyncUntilTermination(
        media::CreateMediaServiceForTesting(std::move(request)));
  }
#endif

#if defined(OS_CHROMEOS)
  if (features::IsSingleProcessMash() &&
      service_name == test_ws::mojom::kServiceName) {
    service_manager::Service::RunAsyncUntilTermination(
        ws::test::CreateInProcessWindowService(
            GetContextFactory(), GetContextFactoryPrivate(),
            CreateGpuInterfaceProvider(), std::move(request)));
  }
#endif
}

bool ShellContentBrowserClient::ShouldTerminateOnServiceQuit(
    const service_manager::Identity& id) {
  if (should_terminate_on_service_quit_callback_)
    return should_terminate_on_service_quit_callback_.Run(id);
  return false;
}

base::Optional<service_manager::Manifest>
ShellContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetContentBrowserOverlayManifest();
  if (name == content::mojom::kPackagedServicesServiceName)
    return GetContentPackagedServicesOverlayManifest();
  if (name == content::mojom::kGpuServiceName)
    return GetContentGpuOverlayManifest();
  if (name == content::mojom::kRendererServiceName)
    return GetContentRendererOverlayManifest();
  if (name == content::mojom::kUtilityServiceName)
    return GetContentUtilityOverlayManifest();

  return base::nullopt;
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
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
          switches::kRegisterFontFiles)) {
    command_line->AppendSwitchASCII(
        switches::kRegisterFontFiles,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kRegisterFontFiles));
  }

#if defined(OS_MACOSX)
  // Needed since on Mac, content_browsertests doesn't use
  // content_test_launcher.cc and instead uses shell_main.cc. So give a signal
  // to shell_main.cc that it's a browser test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBrowserTest)) {
    command_line->AppendSwitch(switches::kBrowserTest);
  }
#endif
}

void ShellContentBrowserClient::AdjustUtilityServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {
#if defined(OS_CHROMEOS)
  if (identity.name() == test_ws::mojom::kServiceName)
    command_line->AppendSwitch(switches::kMessageLoopTypeUi);
#endif
}

std::string ShellContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return ShellURLRequestContextGetter::GetAcceptLanguages();
}

void ShellContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new ResourceDispatcherHostDelegate());
  ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

WebContentsViewDelegate* ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return CreateShellWebContentsViewDelegate(web_contents);
}

QuotaPermissionContext*
ShellContentBrowserClient::CreateQuotaPermissionContext() {
  return new ShellQuotaPermissionContext();
}

void ShellContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  std::move(callback).Run(storage::GetHardCodedSettings(100 * 1024 * 1024));
}

GeneratedCodeCacheSettings
ShellContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

void ShellContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {
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

DevToolsManagerDelegate*
ShellContentBrowserClient::GetDevToolsManagerDelegate() {
  return new ShellDevToolsManagerDelegate(browser_context());
}

void ShellContentBrowserClient::OpenURL(
    SiteInstance* site_instance,
    const OpenURLParams& params,
    const base::Callback<void(WebContents*)>& callback) {
  callback.Run(Shell::CreateNewWindow(site_instance->GetBrowserContext(),
                                      params.url, nullptr, gfx::Size())
                   ->web_contents());
}

scoped_refptr<LoginDelegate> ShellContentBrowserClient::CreateLoginDelegate(
    net::AuthChallengeInfo* auth_info,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  if (!login_request_callback_.is_null()) {
    std::move(login_request_callback_).Run();
    return nullptr;
  }

#if !defined(OS_MACOSX)
  // TODO: implement ShellLoginDialog for other platforms, drop this #if
  return nullptr;
#else
  return ShellLoginDialog::Create(auth_info, std::move(auth_required_callback));
#endif
}

std::string ShellContentBrowserClient::GetUserAgent() const {
  return GetShellUserAgent();
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  mappings->ShareWithRegion(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor));
#endif
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

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

network::mojom::NetworkContextPtr
ShellContentBrowserClient::CreateNetworkContext(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  DCHECK(context);
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return nullptr;

  network::mojom::NetworkContextPtr network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = "en-us,en";
  context_params->enable_data_url_support = true;

#if BUILDFLAG(ENABLE_REPORTING)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRunWebTests)) {
    // Configure the Reporting service in a manner expected by certain Web
    // Platform Tests (network-error-logging and reporting-api).
    //
    //   (1) Always send reports (irrespective of BACKGROUND_SYNC permission)
    //   (2) Lower the timeout for sending reports.
    context_params->reporting_delivery_interval =
        kReportingDeliveryIntervalTimeForWebTests;
    context_params->skip_reporting_send_permission_check = true;
  }
#endif

  GetNetworkService()->CreateNetworkContext(MakeRequest(&network_context),
                                            std::move(context_params));
  return network_context;
}

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

ShellBrowserContext*
    ShellContentBrowserClient::off_the_record_browser_context() {
  return shell_browser_main_parts_->off_the_record_browser_context();
}

void ShellContentBrowserClient::ExposeInterfacesToFrame(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry) {}

}  // namespace content
