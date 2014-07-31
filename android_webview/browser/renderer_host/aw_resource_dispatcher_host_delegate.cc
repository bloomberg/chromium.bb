// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

#include <string>

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_login_delegate.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/common/url_constants.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/auto_login_parser/auto_login_parser.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/url_constants.h"

using android_webview::AwContentsIoThreadClient;
using content::BrowserThread;
using content::ResourceType;
using navigation_interception::InterceptNavigationDelegate;

namespace {

base::LazyInstance<android_webview::AwResourceDispatcherHostDelegate>
    g_webview_resource_dispatcher_host_delegate = LAZY_INSTANCE_INITIALIZER;

void SetCacheControlFlag(
    net::URLRequest* request, int flag) {
  const int all_cache_control_flags = net::LOAD_BYPASS_CACHE |
      net::LOAD_VALIDATE_CACHE |
      net::LOAD_PREFERRING_CACHE |
      net::LOAD_ONLY_FROM_CACHE;
  DCHECK((flag & all_cache_control_flags) == flag);
  int load_flags = request->load_flags();
  load_flags &= ~all_cache_control_flags;
  load_flags |= flag;
  request->SetLoadFlags(load_flags);
}

}  // namespace

namespace android_webview {

// Calls through the IoThreadClient to check the embedders settings to determine
// if the request should be cancelled. There may not always be an IoThreadClient
// available for the |render_process_id|, |render_frame_id| pair (in the case of
// newly created pop up windows, for example) and in that case the request and
// the client callbacks will be deferred the request until a client is ready.
class IoThreadClientThrottle : public content::ResourceThrottle {
 public:
  IoThreadClientThrottle(int render_process_id,
                         int render_frame_id,
                         net::URLRequest* request);
  virtual ~IoThreadClientThrottle();

  // From content::ResourceThrottle
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

  void OnIoThreadClientReady(int new_render_process_id,
                             int new_render_frame_id);
  bool MaybeBlockRequest();
  bool ShouldBlockRequest();
  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

 private:
  int render_process_id_;
  int render_frame_id_;
  net::URLRequest* request_;
};

IoThreadClientThrottle::IoThreadClientThrottle(int render_process_id,
                                               int render_frame_id,
                                               net::URLRequest* request)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      request_(request) { }

IoThreadClientThrottle::~IoThreadClientThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  g_webview_resource_dispatcher_host_delegate.Get().
      RemovePendingThrottleOnIoThread(this);
}

const char* IoThreadClientThrottle::GetNameForLogging() const {
  return "IoThreadClientThrottle";
}

void IoThreadClientThrottle::WillStartRequest(bool* defer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(sgurun): This block can be removed when crbug.com/277937 is fixed.
  if (render_frame_id_ < 1) {
    // OPTIONS is used for preflighted requests which are generated internally.
    DCHECK_EQ("OPTIONS", request_->method());
    return;
  }
  DCHECK(render_process_id_);
  *defer = false;

  // Defer all requests of a pop up that is still not associated with Java
  // client so that the client will get a chance to override requests.
  scoped_ptr<AwContentsIoThreadClient> io_client =
      AwContentsIoThreadClient::FromID(render_process_id_, render_frame_id_);
  if (io_client && io_client->PendingAssociation()) {
    *defer = true;
    AwResourceDispatcherHostDelegate::AddPendingThrottle(
        render_process_id_, render_frame_id_, this);
  } else {
    MaybeBlockRequest();
  }
}

void IoThreadClientThrottle::WillRedirectRequest(const GURL& new_url,
                                                 bool* defer) {
  WillStartRequest(defer);
}

void IoThreadClientThrottle::OnIoThreadClientReady(int new_render_process_id,
                                                   int new_render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!MaybeBlockRequest()) {
    controller()->Resume();
  }
}

bool IoThreadClientThrottle::MaybeBlockRequest() {
  if (ShouldBlockRequest()) {
    controller()->CancelWithError(net::ERR_ACCESS_DENIED);
    return true;
  }
  return false;
}

bool IoThreadClientThrottle::ShouldBlockRequest() {
  scoped_ptr<AwContentsIoThreadClient> io_client =
      AwContentsIoThreadClient::FromID(render_process_id_, render_frame_id_);
  if (!io_client)
    return false;

  // Part of implementation of WebSettings.allowContentAccess.
  if (request_->url().SchemeIs(android_webview::kContentScheme) &&
      io_client->ShouldBlockContentUrls()) {
    return true;
  }

  // Part of implementation of WebSettings.allowFileAccess.
  if (request_->url().SchemeIsFile() &&
      io_client->ShouldBlockFileUrls()) {
    const GURL& url = request_->url();
    if (!url.has_path() ||
        // Application's assets and resources are always available.
        (url.path().find(android_webview::kAndroidResourcePath) != 0 &&
         url.path().find(android_webview::kAndroidAssetPath) != 0)) {
      return true;
    }
  }

  if (io_client->ShouldBlockNetworkLoads()) {
    if (request_->url().SchemeIs(url::kFtpScheme)) {
      return true;
    }
    SetCacheControlFlag(request_, net::LOAD_ONLY_FROM_CACHE);
  } else {
    AwContentsIoThreadClient::CacheMode cache_mode = io_client->GetCacheMode();
    switch(cache_mode) {
      case AwContentsIoThreadClient::LOAD_CACHE_ELSE_NETWORK:
        SetCacheControlFlag(request_, net::LOAD_PREFERRING_CACHE);
        break;
      case AwContentsIoThreadClient::LOAD_NO_CACHE:
        SetCacheControlFlag(request_, net::LOAD_BYPASS_CACHE);
        break;
      case AwContentsIoThreadClient::LOAD_CACHE_ONLY:
        SetCacheControlFlag(request_, net::LOAD_ONLY_FROM_CACHE);
        break;
      default:
        break;
    }
  }
  return false;
}

// static
void AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated() {
  content::ResourceDispatcherHost::Get()->SetDelegate(
      &g_webview_resource_dispatcher_host_delegate.Get());
}

AwResourceDispatcherHostDelegate::AwResourceDispatcherHostDelegate()
    : content::ResourceDispatcherHostDelegate() {
}

AwResourceDispatcherHostDelegate::~AwResourceDispatcherHostDelegate() {
}

void AwResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    ResourceType resource_type,
    int child_id,
    int route_id,
    ScopedVector<content::ResourceThrottle>* throttles) {

  AddExtraHeadersIfNeeded(request, resource_context);

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  // We always push the throttles here. Checking the existence of io_client
  // is racy when a popup window is created. That is because RequestBeginning
  // is called whether or not requests are blocked via BlockRequestForRoute()
  // however io_client may or may not be ready at the time depending on whether
  // webcontents is created.
  throttles->push_back(new IoThreadClientThrottle(
      child_id, request_info->GetRenderFrameID(), request));

  // We allow intercepting only navigations within main frames. This
  // is used to post onPageStarted. We handle shouldOverrideUrlLoading
  // via a sync IPC.
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
    throttles->push_back(InterceptNavigationDelegate::CreateThrottleFor(
        request));
}

void AwResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  AddExtraHeadersIfNeeded(request, resource_context);
}


void AwResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    int child_id,
    int route_id,
    int request_id,
    bool is_content_initiated,
    bool must_download,
    ScopedVector<content::ResourceThrottle>* throttles) {
  GURL url(request->url());
  std::string user_agent;
  std::string content_disposition;
  std::string mime_type;
  int64 content_length = request->GetExpectedContentSize();

  request->extra_request_headers().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);


  net::HttpResponseHeaders* response_headers = request->response_headers();
  if (response_headers) {
    response_headers->GetNormalizedHeader("content-disposition",
        &content_disposition);
    response_headers->GetMimeType(&mime_type);
  }

  request->Cancel();

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  scoped_ptr<AwContentsIoThreadClient> io_client =
      AwContentsIoThreadClient::FromID(
          child_id, request_info->GetRenderFrameID());

  // POST request cannot be repeated in general, so prevent client from
  // retrying the same request, even if it is with a GET.
  if ("GET" == request->method() && io_client) {
    io_client->NewDownload(url,
                           user_agent,
                           content_disposition,
                           mime_type,
                           content_length);
  }
}

content::ResourceDispatcherHostLoginDelegate*
    AwResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info,
        net::URLRequest* request) {
  return new AwLoginDelegate(auth_info, request);
}

bool AwResourceDispatcherHostDelegate::HandleExternalProtocol(const GURL& url,
                                                              int child_id,
                                                              int route_id) {
  // The AwURLRequestJobFactory implementation should ensure this method never
  // gets called.
  NOTREACHED();
  return false;
}

void AwResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response,
    IPC::Sender* sender) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info) {
    DLOG(FATAL) << "Started request without associated info: " <<
        request->url();
    return;
  }

  if (request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    // Check for x-auto-login header.
    auto_login_parser::HeaderData header_data;
    if (auto_login_parser::ParserHeaderInResponse(
            request, auto_login_parser::ALLOW_ANY_REALM, &header_data)) {
      scoped_ptr<AwContentsIoThreadClient> io_client =
          AwContentsIoThreadClient::FromID(request_info->GetChildID(),
                                           request_info->GetRenderFrameID());
      if (io_client) {
        io_client->NewLoginRequest(
            header_data.realm, header_data.account, header_data.args);
      }
    }
  }
}

void AwResourceDispatcherHostDelegate::RemovePendingThrottleOnIoThread(
    IoThreadClientThrottle* throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PendingThrottleMap::iterator it = pending_throttles_.find(
      FrameRouteIDPair(throttle->render_process_id(),
                       throttle->render_frame_id()));
  if (it != pending_throttles_.end()) {
    pending_throttles_.erase(it);
  }
}

// static
void AwResourceDispatcherHostDelegate::OnIoThreadClientReady(
    int new_render_process_id,
    int new_render_frame_id) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(
          &AwResourceDispatcherHostDelegate::OnIoThreadClientReadyInternal,
          base::Unretained(
              g_webview_resource_dispatcher_host_delegate.Pointer()),
          new_render_process_id, new_render_frame_id));
}

// static
void AwResourceDispatcherHostDelegate::AddPendingThrottle(
    int render_process_id,
    int render_frame_id,
    IoThreadClientThrottle* pending_throttle) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(
          &AwResourceDispatcherHostDelegate::AddPendingThrottleOnIoThread,
          base::Unretained(
              g_webview_resource_dispatcher_host_delegate.Pointer()),
          render_process_id, render_frame_id, pending_throttle));
}

void AwResourceDispatcherHostDelegate::AddPendingThrottleOnIoThread(
    int render_process_id,
    int render_frame_id_id,
    IoThreadClientThrottle* pending_throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  pending_throttles_.insert(
      std::pair<FrameRouteIDPair, IoThreadClientThrottle*>(
          FrameRouteIDPair(render_process_id, render_frame_id_id),
          pending_throttle));
}

void AwResourceDispatcherHostDelegate::OnIoThreadClientReadyInternal(
    int new_render_process_id,
    int new_render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PendingThrottleMap::iterator it = pending_throttles_.find(
      FrameRouteIDPair(new_render_process_id, new_render_frame_id));

  if (it != pending_throttles_.end()) {
    IoThreadClientThrottle* throttle = it->second;
    throttle->OnIoThreadClientReady(new_render_process_id, new_render_frame_id);
    pending_throttles_.erase(it);
  }
}

void AwResourceDispatcherHostDelegate::AddExtraHeadersIfNeeded(
    net::URLRequest* request,
    content::ResourceContext* resource_context) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info)
    return;
  if (request_info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME)
    return;

  const content::PageTransition transition = request_info->GetPageTransition();
  const bool is_load_url =
      transition & content::PAGE_TRANSITION_FROM_API;
  const bool is_go_back_forward =
      transition & content::PAGE_TRANSITION_FORWARD_BACK;
  const bool is_reload = content::PageTransitionCoreTypeIs(
      transition, content::PAGE_TRANSITION_RELOAD);
  if (is_load_url || is_go_back_forward || is_reload) {
    AwResourceContext* awrc = static_cast<AwResourceContext*>(resource_context);
    std::string extra_headers = awrc->GetExtraHeaders(request->url());
    if (!extra_headers.empty()) {
      net::HttpRequestHeaders headers;
      headers.AddHeadersFromString(extra_headers);
      for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext(); ) {
        request->SetExtraRequestHeaderByName(it.name(), it.value(), false);
      }
    }
  }
}

}  // namespace android_webview
