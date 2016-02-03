// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_contents_client_bridge_base.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_locale_manager.h"
#include "android_webview/browser/aw_printing_message_filter.h"
#include "android_webview/browser/aw_quota_permission_context.h"
#include "android_webview/browser/aw_web_preferences_populater.h"
#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/net_disk_cache_remover.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/path_service.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/crash/content/browser/crash_micro_dump_manager_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "net/android/network_library.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/resources/grit/ui_resources.h"

using content::BrowserThread;
using content::ResourceType;

namespace android_webview {
namespace {

// TODO(sgurun) move this to its own file.
// This class filters out incoming aw_contents related IPC messages for the
// renderer process on the IPC thread.
class AwContentsMessageFilter : public content::BrowserMessageFilter {
public:
  explicit AwContentsMessageFilter(int process_id);

  // BrowserMessageFilter methods.
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnShouldOverrideUrlLoading(int routing_id,
                                  const base::string16& url,
                                  bool has_user_gesture,
                                  bool is_redirect,
                                  bool is_main_frame,
                                  bool* ignore_navigation);
  void OnSubFrameCreated(int parent_render_frame_id, int child_render_frame_id);

private:
 ~AwContentsMessageFilter() override;

  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(AwContentsMessageFilter);
};

AwContentsMessageFilter::AwContentsMessageFilter(int process_id)
    : BrowserMessageFilter(AndroidWebViewMsgStart),
      process_id_(process_id) {
}

AwContentsMessageFilter::~AwContentsMessageFilter() {
}

void AwContentsMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == AwViewHostMsg_ShouldOverrideUrlLoading::ID) {
    *thread = BrowserThread::UI;
  }
}

bool AwContentsMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwContentsMessageFilter, message)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_ShouldOverrideUrlLoading,
                        OnShouldOverrideUrlLoading)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_SubFrameCreated, OnSubFrameCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwContentsMessageFilter::OnShouldOverrideUrlLoading(
    int render_frame_id,
    const base::string16& url,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    bool* ignore_navigation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  *ignore_navigation = false;
  AwContentsClientBridgeBase* client =
      AwContentsClientBridgeBase::FromID(process_id_, render_frame_id);
  if (client) {
    *ignore_navigation = client->ShouldOverrideUrlLoading(
        url, has_user_gesture, is_redirect, is_main_frame);
  } else {
    LOG(WARNING) << "Failed to find the associated render view host for url: "
                 << url;
  }
}

void AwContentsMessageFilter::OnSubFrameCreated(int parent_render_frame_id,
                                                int child_render_frame_id) {
  AwContentsIoThreadClient::SubFrameCreated(
      process_id_, parent_render_frame_id, child_render_frame_id);
}

class AwAccessTokenStore : public content::AccessTokenStore {
 public:
  AwAccessTokenStore() { }

  // content::AccessTokenStore implementation
  void LoadAccessTokens(const LoadAccessTokensCallbackType& request) override {
    AccessTokenStore::AccessTokenSet access_token_set;
    // AccessTokenSet and net::URLRequestContextGetter not used on Android,
    // but Run needs to be called to finish the geolocation setup.
    request.Run(access_token_set, NULL);
  }
  void SaveAccessToken(const GURL& server_url,
                       const base::string16& access_token) override {}

 private:
  ~AwAccessTokenStore() override {}

  DISALLOW_COPY_AND_ASSIGN(AwAccessTokenStore);
};

AwLocaleManager* g_locale_manager = NULL;

}  // anonymous namespace

// static
std::string AwContentBrowserClient::GetAcceptLangsImpl() {
  // Start with the current locale.
  std::string langs = g_locale_manager->GetLocale();

  // If we're not en-US, add in en-US which will be
  // used with a lower q-value.
  if (base::ToLowerASCII(langs) != "en-us") {
    langs += ",en-US";
  }
  return langs;
}

// static
AwBrowserContext* AwContentBrowserClient::GetAwBrowserContext() {
  return AwBrowserContext::GetDefault();
}

AwContentBrowserClient::AwContentBrowserClient(
    JniDependencyFactory* native_factory)
    : native_factory_(native_factory) {
  base::FilePath user_data_dir;
  if (!PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  browser_context_.reset(
      new AwBrowserContext(user_data_dir, native_factory_));
  g_locale_manager = native_factory->CreateAwLocaleManager();
}

AwContentBrowserClient::~AwContentBrowserClient() {
  delete g_locale_manager;
  g_locale_manager = NULL;
}

void AwContentBrowserClient::AddCertificate(net::CertificateMimeType cert_type,
                                            const void* cert_data,
                                            size_t cert_size,
                                            int render_process_id,
                                            int render_frame_id) {
  if (cert_size > 0)
    net::android::StoreCertificate(cert_type, cert_data, cert_size);
}

content::BrowserMainParts* AwContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return new AwBrowserMainParts(browser_context_.get());
}

content::WebContentsViewDelegate*
AwContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return native_factory_->CreateViewDelegate(web_contents);
}

void AwContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  // Grant content: scheme access to the whole renderer process, since we impose
  // per-view access checks, and access is granted by default (see
  // AwSettings.mAllowContentUrlAccess).
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), url::kContentScheme);

  host->AddFilter(new AwContentsMessageFilter(host->GetID()));
  host->AddFilter(new cdm::CdmMessageFilterAndroid());
  host->AddFilter(new AwPrintingMessageFilter(host->GetID()));
}

net::URLRequestContextGetter* AwContentBrowserClient::CreateRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK_EQ(browser_context_.get(), browser_context);
  return browser_context_->CreateRequestContext(
      protocol_handlers, std::move(request_interceptors));
}

net::URLRequestContextGetter*
AwContentBrowserClient::CreateRequestContextForStoragePartition(
    content::BrowserContext* browser_context,
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK_EQ(browser_context_.get(), browser_context);
  // TODO(mkosiba,kinuko): request_interceptors should be hooked up in the
  // downstream. (crbug.com/350286)
  return browser_context_->CreateRequestContextForStoragePartition(
      partition_path, in_memory, protocol_handlers,
      std::move(request_interceptors));
}

bool AwContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  const std::string scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  // See CreateJobFactory in aw_url_request_context_getter.cc for the
  // list of protocols that are handled.
  // TODO(mnaganov): Make this automatic.
  static const char* const kProtocolList[] = {
    url::kDataScheme,
    url::kBlobScheme,
    url::kFileSystemScheme,
    content::kChromeUIScheme,
    content::kChromeDevToolsScheme,
    url::kContentScheme,
  };
  if (scheme == url::kFileScheme) {
    // Return false for the "special" file URLs, so they can be loaded
    // even if access to file: scheme is not granted to the child process.
    return !IsAndroidSpecialFileUrl(url);
  }
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }
  return net::URLRequest::IsHandledProtocol(scheme);
}

std::string AwContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return alias_name;
}

void AwContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    NOTREACHED() << "Android WebView does not support multi-process yet";
  } else {
    // The only kind of a child process WebView can have is renderer.
    DCHECK_EQ(switches::kRendererProcess,
              command_line->GetSwitchValueASCII(switches::kProcessType));

    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    static const char* const kCommonSwitchNames[] = {
      switches::kDisablePageVisibility,
    };
    command_line->CopySwitchesFrom(browser_command_line, kCommonSwitchNames,
                                   arraysize(kCommonSwitchNames));
  }
}

std::string AwContentBrowserClient::GetApplicationLocale() {
  return base::android::GetDefaultLocale();
}

std::string AwContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return GetAcceptLangsImpl();
}

const gfx::ImageSkia* AwContentBrowserClient::GetDefaultFavicon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // TODO(boliu): Bundle our own default favicon?
  return rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
}

bool AwContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                           const GURL& first_party,
                           content::ResourceContext* context) {
  // WebView doesn't have a per-site policy for locally stored data,
  // instead AppCache can be disabled for individual WebViews.
  return true;
}


bool AwContentBrowserClient::AllowGetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const net::CookieList& cookie_list,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_frame_id) {
  return AwCookieAccessPolicy::GetInstance()->AllowGetCookie(url,
                                                             first_party,
                                                             cookie_list,
                                                             context,
                                                             render_process_id,
                                                             render_frame_id);
}

bool AwContentBrowserClient::AllowSetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const std::string& cookie_line,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_frame_id,
                                            const net::CookieOptions& options) {
  return AwCookieAccessPolicy::GetInstance()->AllowSetCookie(url,
                                                             first_party,
                                                             cookie_line,
                                                             context,
                                                             render_process_id,
                                                             render_frame_id,
                                                             options);
}

bool AwContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  // Android WebView does not yet support web workers.
  return false;
}

void AwContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames,
    base::Callback<void(bool)> callback) {
  // Android WebView does not yet support web workers.
  callback.Run(false);
}

bool AwContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  // Android WebView does not yet support web workers.
  return false;
}

content::QuotaPermissionContext*
AwContentBrowserClient::CreateQuotaPermissionContext() {
  return new AwQuotaPermissionContext;
}

void AwContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ResourceType resource_type,
    bool overridable,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(bool)>& callback,
    content::CertificateRequestResultType* result) {
  AwContentsClientBridgeBase* client =
      AwContentsClientBridgeBase::FromWebContents(web_contents);
  bool cancel_request = true;
  if (client)
    client->AllowCertificateError(cert_error,
                                  ssl_info.cert.get(),
                                  request_url,
                                  callback,
                                  &cancel_request);
  if (cancel_request)
    *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY;
}

void AwContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    scoped_ptr<content::ClientCertificateDelegate> delegate) {
  AwContentsClientBridgeBase* client =
      AwContentsClientBridgeBase::FromWebContents(web_contents);
  if (client)
    client->SelectClientCertificate(cert_request_info, std::move(delegate));
}

bool AwContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    const blink::WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    content::ResourceContext* context,
    int render_process_id,
    int opener_render_view_id,
    int opener_render_frame_id,
    bool* no_javascript_access) {
  // We unconditionally allow popup windows at this stage and will give
  // the embedder the opporunity to handle displaying of the popup in
  // WebContentsDelegate::AddContents (via the
  // AwContentsClient.onCreateWindow callback).
  // Note that if the embedder has blocked support for creating popup
  // windows through AwSettings, then we won't get to this point as
  // the popup creation will have been blocked at the WebKit level.
  if (no_javascript_access) {
    *no_javascript_access = false;
  }
  return true;
}

void AwContentBrowserClient::ResourceDispatcherHostCreated() {
  AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated();
}

net::NetLog* AwContentBrowserClient::GetNetLog() {
  return browser_context_->GetAwURLRequestContext()->GetNetLog();
}

content::AccessTokenStore* AwContentBrowserClient::CreateAccessTokenStore() {
  return new AwAccessTokenStore();
}

bool AwContentBrowserClient::IsFastShutdownPossible() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    NOTREACHED()
        << "Android WebView is single process, so IsFastShutdownPossible"
        << " should never be called";
    return false;
  } else {
    return true;
  }
}

void AwContentBrowserClient::ClearCache(content::RenderFrameHost* rfh) {
  RemoveHttpDiskCache(rfh->GetProcess()->GetBrowserContext(),
                      rfh->GetProcess()->GetID());
}

void AwContentBrowserClient::ClearCookies(content::RenderFrameHost* rfh) {
  // TODO(boliu): Implement.
  NOTIMPLEMENTED();
}

base::FilePath AwContentBrowserClient::GetDefaultDownloadDirectory() {
  // Android WebView does not currently use the Chromium downloads system.
  // Download requests are cancelled immedately when recognized; see
  // AwResourceDispatcherHost::CreateResourceHandlerForDownload. However the
  // download system still tries to start up and calls this before recognizing
  // the request has been cancelled.
  return base::FilePath();
}

std::string AwContentBrowserClient::GetDefaultDownloadName() {
  NOTREACHED() << "Android WebView does not use chromium downloads";
  return std::string();
}

void AwContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  NOTREACHED() << "Android WebView does not support plugins";
}

bool AwContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
  NOTREACHED() << "Android WebView does not support plugins";
  return false;
}

void AwContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings,
      std::map<int, base::MemoryMappedFile::Region>* regions) {
  int fd = ui::GetMainAndroidPackFd(
      &(*regions)[kAndroidWebViewMainPakDescriptor]);
  mappings->Share(kAndroidWebViewMainPakDescriptor, fd);

  fd = ui::GetLocalePackFd(&(*regions)[kAndroidWebViewLocalePakDescriptor]);
  mappings->Share(kAndroidWebViewLocalePakDescriptor, fd);

  base::ScopedFD crash_signal_file =
      breakpad::CrashMicroDumpManager::GetInstance()->CreateCrashInfoChannel(
          child_process_id);
  if (crash_signal_file.is_valid()) {
    mappings->Transfer(kAndroidWebViewCrashSignalDescriptor,
                       std::move(crash_signal_file));
  }
}

void AwContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  if (!preferences_populater_.get()) {
    preferences_populater_ = make_scoped_ptr(native_factory_->
        CreateWebPreferencesPopulater());
  }
  preferences_populater_->PopulateFor(
      content::WebContents::FromRenderViewHost(rvh), web_prefs);
}

ScopedVector<content::NavigationThrottle>
AwContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  ScopedVector<content::NavigationThrottle> throttles;
  // We allow intercepting only navigations within main frames. This
  // is used to post onPageStarted. We handle shouldOverrideUrlLoading
  // via a sync IPC.
  if (navigation_handle->IsInMainFrame()) {
    throttles.push_back(
        navigation_interception::InterceptNavigationDelegate::CreateThrottleFor(
            navigation_handle));
  }
  return throttles;
}

#if defined(VIDEO_HOLE)
content::ExternalVideoSurfaceContainer*
AwContentBrowserClient::OverrideCreateExternalVideoSurfaceContainer(
    content::WebContents* web_contents) {
  return native_factory_->CreateExternalVideoSurfaceContainer(web_contents);
}
#endif

}  // namespace android_webview
