// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "android_webview/browser/aw_contents_client_bridge_base.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_dev_tools_manager_delegate.h"
#include "android_webview/browser/aw_quota_permission_context.h"
#include "android_webview/browser/aw_web_preferences_populater.h"
#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/net_disk_cache_remover.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/url_constants.h"
#include "base/base_paths_android.h"
#include "base/path_service.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "net/android/network_library.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/base/resource/resource_bundle.h"
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
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(
      const IPC::Message& message) OVERRIDE;

  void OnShouldOverrideUrlLoading(int routing_id,
                                  const base::string16& url,
                                  bool* ignore_navigation);
  void OnSubFrameCreated(int parent_render_frame_id, int child_render_frame_id);

private:
  virtual ~AwContentsMessageFilter();

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
    const IPC::Message& message, BrowserThread::ID* thread) {
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
    bool* ignore_navigation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  *ignore_navigation = false;
  AwContentsClientBridgeBase* client =
      AwContentsClientBridgeBase::FromID(process_id_, render_frame_id);
  if (client) {
    *ignore_navigation = client->ShouldOverrideUrlLoading(url);
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
  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& request) OVERRIDE {
    AccessTokenStore::AccessTokenSet access_token_set;
    // AccessTokenSet and net::URLRequestContextGetter not used on Android,
    // but Run needs to be called to finish the geolocation setup.
    request.Run(access_token_set, NULL);
  }
  virtual void SaveAccessToken(const GURL& server_url,
                               const base::string16& access_token) OVERRIDE { }

 private:
  virtual ~AwAccessTokenStore() { }

  DISALLOW_COPY_AND_ASSIGN(AwAccessTokenStore);
};

void CancelProtectedMediaIdentifierPermissionRequests(
    int render_process_id,
    int render_view_id,
    const GURL& origin) {
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (delegate)
    delegate->CancelProtectedMediaIdentifierPermissionRequests(origin);
}

void CancelGeolocationPermissionRequests(
    int render_process_id,
    int render_view_id,
    const GURL& origin) {
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (delegate)
    delegate->CancelGeolocationPermissionRequests(origin);
}

}  // namespace

std::string AwContentBrowserClient::GetAcceptLangsImpl() {
  // Start with the currnet locale.
  std::string langs = l10n_util::GetDefaultLocale();

  // If we're not en-US, add in en-US which will be
  // used with a lower q-value.
  if (base::StringToLowerASCII(langs) != "en-us") {
    langs += ",en-US";
  }
  return langs;
}

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
}

AwContentBrowserClient::~AwContentBrowserClient() {
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
  // If WebView becomes multi-process capable, this may be insecure.
  // More benefit can be derived from the ChildProcessSecurotyPolicy by
  // deferring the GrantScheme calls until we know that a given child process
  // really does need that priviledge. Check here to ensure we rethink this
  // when the time comes. See crbug.com/156062.
  CHECK(content::RenderProcessHost::run_renderer_in_process());

  // Grant content: and file: scheme to the whole process, since we impose
  // per-view access checks.
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), android_webview::kContentScheme);
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), url::kFileScheme);

  host->AddFilter(new AwContentsMessageFilter(host->GetID()));
  host->AddFilter(new cdm::CdmMessageFilterAndroid());
}

net::URLRequestContextGetter* AwContentBrowserClient::CreateRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK_EQ(browser_context_.get(), browser_context);
  return browser_context_->CreateRequestContext(protocol_handlers,
                                                request_interceptors.Pass());
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
      request_interceptors.Pass());
}

std::string AwContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return alias_name;
}

void AwContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  NOTREACHED() << "Android WebView does not support multi-process yet";
}

std::string AwContentBrowserClient::GetApplicationLocale() {
  return l10n_util::GetDefaultLocale();
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
                                            net::CookieOptions* options) {
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
    unsigned long estimated_size,
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
    int render_process_id,
    int render_frame_id,
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
      AwContentsClientBridgeBase::FromID(render_process_id, render_frame_id);
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
      int render_process_id,
      int render_frame_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) {
  AwContentsClientBridgeBase* client =
      AwContentsClientBridgeBase::FromID(render_process_id, render_frame_id);
  if (client) {
    client->SelectClientCertificate(cert_request_info, callback);
  } else {
    callback.Run(NULL);
  }
}

blink::WebNotificationPermission
    AwContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_url,
        content::ResourceContext* context,
        int render_process_id) {
  // Android WebView does not support notifications, so return Denied here.
  return blink::WebNotificationPermissionDenied;
}

void AwContentBrowserClient::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
    content::RenderFrameHost* render_frame_host,
    scoped_ptr<content::DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  NOTREACHED() << "Android WebView does not support desktop notifications.";
}

void AwContentBrowserClient::RequestGeolocationPermission(
    content::WebContents* web_contents,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> result_callback,
    base::Closure* cancel_callback) {
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (delegate == NULL) {
    DVLOG(0) << "Dropping GeolocationPermission request";
    result_callback.Run(false);
    return;
  }

  GURL origin = requesting_frame.GetOrigin();
  if (cancel_callback) {
    *cancel_callback = base::Bind(
        CancelGeolocationPermissionRequests, render_process_id, render_view_id,
        origin);
  }
  delegate->RequestGeolocationPermission(origin, result_callback);
}

void AwContentBrowserClient::RequestMidiSysExPermission(
    content::WebContents* web_contents,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> result_callback,
    base::Closure* cancel_callback) {
  // TODO(toyoshim): Android WebView is not supported yet.
  // See http://crbug.com/339767.
  result_callback.Run(false);
}

void AwContentBrowserClient::RequestProtectedMediaIdentifierPermission(
    content::WebContents* web_contents,
    const GURL& origin,
    base::Callback<void(bool)> result_callback,
    base::Closure* cancel_callback) {
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (delegate == NULL) {
    DVLOG(0) << "Dropping ProtectedMediaIdentifierPermission request";
    result_callback.Run(false);
    return;
  }

  if (cancel_callback) {
    *cancel_callback = base::Bind(
        CancelProtectedMediaIdentifierPermissionRequests,
        render_process_id, render_view_id, origin);
  }
  delegate->RequestProtectedMediaIdentifierPermission(origin, result_callback);
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
    int opener_id,
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
  NOTREACHED() << "Android WebView is single process, so IsFastShutdownPossible"
               << " should never be called";
  return false;
}

void AwContentBrowserClient::ClearCache(content::RenderViewHost* rvh) {
  RemoveHttpDiskCache(rvh->GetProcess()->GetBrowserContext(),
                      rvh->GetProcess()->GetID());
}

void AwContentBrowserClient::ClearCookies(content::RenderViewHost* rvh) {
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

void AwContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    const GURL& url,
    content::WebPreferences* web_prefs) {
  if (!preferences_populater_.get()) {
    preferences_populater_ = make_scoped_ptr(native_factory_->
        CreateWebPreferencesPopulater());
  }
  preferences_populater_->PopulateFor(
      content::WebContents::FromRenderViewHost(rvh), web_prefs);
}

#if defined(VIDEO_HOLE)
content::ExternalVideoSurfaceContainer*
AwContentBrowserClient::OverrideCreateExternalVideoSurfaceContainer(
    content::WebContents* web_contents) {
  return native_factory_->CreateExternalVideoSurfaceContainer(web_contents);
}
#endif

content::DevToolsManagerDelegate*
AwContentBrowserClient::GetDevToolsManagerDelegate() {
  return new AwDevToolsManagerDelegate();
}

}  // namespace android_webview
