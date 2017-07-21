// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/memory_coordinator_delegate.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "media/audio/audio_manager.h"
#include "media/base/cdm_factory.h"
#include "media/media_features.h"
#include "net/ssl/client_cert_identity.h"
#include "storage/browser/quota/quota_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return nullptr;
}

void ContentBrowserClient::PostAfterStartupTask(
    const tracked_objects::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::OnceClosure task) {
  task_runner->PostTask(from_here, std::move(task));
}

bool ContentBrowserClient::IsBrowserStartupComplete() {
  return true;
}

WebContentsViewDelegate* ContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return nullptr;
}

GURL ContentBrowserClient::GetEffectiveURL(BrowserContext* browser_context,
                                           const GURL& url) {
  return url;
}

bool ContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool ContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  return false;
}

bool ContentBrowserClient::ShouldLockToOrigin(BrowserContext* browser_context,
                                              const GURL& effective_url) {
  return true;
}

void ContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);
}

bool ContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) const {
  return false;
}

bool ContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool ContentBrowserClient::CanCommitURL(RenderProcessHost* process_host,
                                        const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::ShouldAllowOpenURL(SiteInstance* site_instance,
                                              const GURL& url) {
  return true;
}

bool ContentBrowserClient::
    ShouldFrameShareParentSiteInstanceDespiteTopDocumentIsolation(
        const GURL& url,
        SiteInstance* parent_site_instance) {
  return false;
}

bool ContentBrowserClient::IsSuitableHost(RenderProcessHost* process_host,
                                          const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::MayReuseHost(RenderProcessHost* process_host) {
  return true;
}

bool ContentBrowserClient::ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url) {
  return false;
}

bool ContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

std::unique_ptr<media::AudioManager> ContentBrowserClient::CreateAudioManager(
    media::AudioLogFactory* audio_log_factory) {
  return nullptr;
}

std::unique_ptr<media::CdmFactory> ContentBrowserClient::CreateCdmFactory() {
  return nullptr;
}

bool ContentBrowserClient::ShouldSwapProcessesForRedirect(
    BrowserContext* browser_context,
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

bool ContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return true;
}

std::vector<url::Origin>
ContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  return std::vector<url::Origin>();
}

std::string ContentBrowserClient::GetApplicationLocale() {
  return "en-US";
}

std::string ContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return std::string();
}

const gfx::ImageSkia* ContentBrowserClient::GetDefaultFavicon() {
  static gfx::ImageSkia* empty = new gfx::ImageSkia();
  return empty;
}

base::FilePath ContentBrowserClient::GetLoggingFileName() {
  return base::FilePath();
}

bool ContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                                         const GURL& first_party,
                                         ResourceContext* context) {
  return true;
}

bool ContentBrowserClient::AllowServiceWorker(
    const GURL& scope,
    const GURL& first_party,
    ResourceContext* context,
    const base::Callback<WebContents*(void)>& wc_getter) {
  return true;
}

bool ContentBrowserClient::IsDataSaverEnabled(BrowserContext* context) {
  return false;
}

bool ContentBrowserClient::AllowGetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CookieList& cookie_list,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  return true;
}

bool ContentBrowserClient::AllowSetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const std::string& cookie_line,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id,
                                          const net::CookieOptions& options) {
  return true;
}

void ContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames,
    base::Callback<void(bool)> callback) {
  callback.Run(true);
}

bool ContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  return true;
}

ContentBrowserClient::AllowWebBluetoothResult
ContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return AllowWebBluetoothResult::ALLOW;
}

std::string ContentBrowserClient::GetWebBluetoothBlocklist() {
  return std::string();
}

QuotaPermissionContext* ContentBrowserClient::CreateQuotaPermissionContext() {
  return nullptr;
}

void ContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  // By default, no quota is provided, embedders should override.
  std::move(callback).Run(storage::GetNoQuotaSettings());
}

void ContentBrowserClient::AllowCertificateError(
    WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ResourceType resource_type,
    bool overridable,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(CertificateRequestResultType)>& callback) {
  callback.Run(CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

void ContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {}

net::URLRequestContext* ContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, ResourceContext* context) {
  return nullptr;
}

std::string ContentBrowserClient::GetStoragePartitionIdForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  return std::string();
}

bool ContentBrowserClient::IsValidStoragePartitionId(
    BrowserContext* browser_context,
    const std::string& partition_id) {
  // Since the GetStoragePartitionIdForChildProcess() only generates empty
  // strings, we should only ever see empty strings coming back.
  return partition_id.empty();
}

void ContentBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site,
    bool can_be_default,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  partition_domain->clear();
  partition_name->clear();
  *in_memory = false;
}

MediaObserver* ContentBrowserClient::GetMediaObserver() {
  return nullptr;
}

PlatformNotificationService*
ContentBrowserClient::GetPlatformNotificationService() {
  return nullptr;
}

bool ContentBrowserClient::CanCreateWindow(
    RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

SpeechRecognitionManagerDelegate*
    ContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return nullptr;
}

net::NetLog* ContentBrowserClient::GetNetLog() {
  return nullptr;
}

base::FilePath ContentBrowserClient::GetDefaultDownloadDirectory() {
  return base::FilePath();
}

std::string ContentBrowserClient::GetDefaultDownloadName() {
  return std::string();
}

base::FilePath ContentBrowserClient::GetShaderDiskCacheDirectory() {
  return base::FilePath();
}

BrowserPpapiHost*
    ContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
  return nullptr;
}

gpu::GpuChannelEstablishFactory*
ContentBrowserClient::GetGpuChannelEstablishFactory() {
  return nullptr;
}

bool ContentBrowserClient::AllowPepperSocketAPI(
    BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const SocketPermissionRequest* params) {
  return false;
}

bool ContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

std::unique_ptr<VpnServiceProxy> ContentBrowserClient::GetVpnServiceProxy(
    BrowserContext* browser_context) {
  return nullptr;
}

ui::SelectFilePolicy* ContentBrowserClient::CreateSelectFilePolicy(
    WebContents* web_contents) {
  return nullptr;
}

DevToolsManagerDelegate* ContentBrowserClient::GetDevToolsManagerDelegate() {
  return nullptr;
}

TracingDelegate* ContentBrowserClient::GetTracingDelegate() {
  return nullptr;
}

bool ContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

bool ContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

std::string ContentBrowserClient::GetServiceUserIdForBrowserContext(
    BrowserContext* browser_context) {
  return base::GenerateGUID();
}

bool ContentBrowserClient::BindAssociatedInterfaceRequestFromFrame(
    RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return false;
}

ControllerPresentationServiceDelegate*
ContentBrowserClient::GetControllerPresentationServiceDelegate(
    WebContents* web_contents) {
  return nullptr;
}

ReceiverPresentationServiceDelegate*
ContentBrowserClient::GetReceiverPresentationServiceDelegate(
    WebContents* web_contents) {
  return nullptr;
}

void ContentBrowserClient::OpenURL(
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const base::Callback<void(content::WebContents*)>& callback) {
  callback.Run(nullptr);
}

std::string ContentBrowserClient::GetMetricSuffixForURL(const GURL& url) {
  return std::string();
}

std::vector<std::unique_ptr<NavigationThrottle>>
ContentBrowserClient::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  return std::vector<std::unique_ptr<NavigationThrottle>>();
}

std::unique_ptr<NavigationUIData> ContentBrowserClient::GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  return nullptr;
}

#if defined(OS_WIN)
bool ContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy) {
  return true;
}

base::string16 ContentBrowserClient::GetAppContainerSidForSandboxType(
    int sandbox_type) const {
  // Embedders should override this method and return different SIDs for each
  // sandbox type. Note: All content level tests will run child processes in the
  // same AppContainer.
  return base::string16(
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-129201922");
}
#endif  // defined(OS_WIN)

std::unique_ptr<base::Value> ContentBrowserClient::GetServiceManifestOverlay(
    base::StringPiece name) {
  return nullptr;
}

std::vector<ContentBrowserClient::ServiceManifestInfo>
ContentBrowserClient::GetExtraServiceManifests() {
  return std::vector<ContentBrowserClient::ServiceManifestInfo>();
}

std::unique_ptr<MemoryCoordinatorDelegate>
ContentBrowserClient::GetMemoryCoordinatorDelegate() {
  return std::unique_ptr<MemoryCoordinatorDelegate>();
}

::rappor::RapporService* ContentBrowserClient::GetRapporService() {
  return nullptr;
}

std::unique_ptr<base::TaskScheduler::InitParams>
ContentBrowserClient::GetTaskSchedulerInitParams() {
  return nullptr;
}

std::vector<std::unique_ptr<URLLoaderThrottle>>
ContentBrowserClient::CreateURLLoaderThrottles(
    const base::Callback<WebContents*()>& wc_getter) {
  return std::vector<std::unique_ptr<URLLoaderThrottle>>();
}

}  // namespace content
