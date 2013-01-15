// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

#include <string>

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_login_delegate.h"
#include "android_webview/common/url_constants.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"

using android_webview::AwContentsIoThreadClient;
using content::BrowserThread;
using content::InterceptNavigationDelegate;

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
  request->set_load_flags(load_flags);
}

}  // namespace

namespace android_webview {

// Calls through the IoThreadClient to check the embedders settings to determine
// if the request should be cancelled. There may not always be an IoThreadClient
// available for the |child_id|, |route_id| pair (in the case of newly created
// pop up windows, for example) and in that case the request and the client
// callbacks will be deferred the request until a client is ready.
class IoThreadClientThrottle : public content::ResourceThrottle {
 public:
  IoThreadClientThrottle(int child_id,
                         int route_id,
                         net::URLRequest* request);
  virtual ~IoThreadClientThrottle();

  // From content::ResourceThrottle
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

  bool MaybeDeferRequest(bool* defer);
  void OnIoThreadClientReady(int new_child_id, int new_route_id);
  bool MaybeBlockRequest();
  bool ShouldBlockRequest();
  scoped_ptr<AwContentsIoThreadClient> GetIoThreadClient();
  int get_child_id() const { return child_id_; }
  int get_route_id() const { return route_id_; }

private:
  int child_id_;
  int route_id_;
  net::URLRequest* request_;
};

IoThreadClientThrottle::IoThreadClientThrottle(int child_id,
                                               int route_id,
                                               net::URLRequest* request)
    : child_id_(child_id),
      route_id_(route_id),
      request_(request) { }

IoThreadClientThrottle::~IoThreadClientThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  g_webview_resource_dispatcher_host_delegate.Get().
      RemovePendingThrottleOnIoThread(this);
}

void IoThreadClientThrottle::WillStartRequest(bool* defer) {
  if (!MaybeDeferRequest(defer)) {
    MaybeBlockRequest();
  }
}

void IoThreadClientThrottle::WillRedirectRequest(const GURL& new_url,
                                                 bool* defer) {
  WillStartRequest(defer);
}

bool IoThreadClientThrottle::MaybeDeferRequest(bool* defer) {
  scoped_ptr<AwContentsIoThreadClient> io_client =
      AwContentsIoThreadClient::FromID(child_id_, route_id_);
  *defer = false;
  if (!io_client.get()) {
    *defer = true;
    AwResourceDispatcherHostDelegate::AddPendingThrottle(
        child_id_, route_id_, this);
  }
  return *defer;
}

void IoThreadClientThrottle::OnIoThreadClientReady(int new_child_id,
                                                   int new_route_id) {
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
      AwContentsIoThreadClient::FromID(child_id_, route_id_);
  DCHECK(io_client.get());

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
    if (request_->url().SchemeIs(chrome::kFtpScheme)) {
      return true;
    }
    SetCacheControlFlag(request_, net::LOAD_ONLY_FROM_CACHE);
  } else {
    AwContentsIoThreadClient::CacheMode cache_mode =
        GetIoThreadClient()->GetCacheMode();
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

scoped_ptr<AwContentsIoThreadClient>
    IoThreadClientThrottle::GetIoThreadClient() {
  return AwContentsIoThreadClient::FromID(child_id_, route_id_);
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
    appcache::AppCacheService* appcache_service,
    ResourceType::Type resource_type,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request,
    ScopedVector<content::ResourceThrottle>* throttles) {

  throttles->push_back(new IoThreadClientThrottle(
      child_id, route_id, request));

  bool allow_intercepting =
      // We ignore POST requests because of BUG=155250.
      request->method() != "POST" &&
      // We allow intercepting navigations within subframes, but only if the
      // scheme other than http or https. This is because the embedder
      // can't distinguish main frame and subframe callbacks (which could lead
      // to broken content if the embedder decides to not ignore the main frame
      // navigation, but ignores the subframe navigation).
      // The reason this is supported at all is that certain JavaScript-based
      // frameworks use iframe navigation as a form of communication with the
      // embedder.
      (resource_type == ResourceType::MAIN_FRAME ||
       (resource_type == ResourceType::SUB_FRAME &&
        !request->url().SchemeIs(chrome::kHttpScheme) &&
        !request->url().SchemeIs(chrome::kHttpsScheme)));
  if (allow_intercepting) {
    throttles->push_back(InterceptNavigationDelegate::CreateThrottleFor(
        request));
  }
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

  request->GetResponseHeaderByName("content-disposition", &content_disposition);
  request->extra_request_headers().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);
  request->GetMimeType(&mime_type);

  request->Cancel();

  scoped_ptr<AwContentsIoThreadClient> io_client =
      AwContentsIoThreadClient::FromID(child_id, route_id);

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

bool AwResourceDispatcherHostDelegate::AcceptAuthRequest(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  return true;
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

void AwResourceDispatcherHostDelegate::RemovePendingThrottleOnIoThread(
    IoThreadClientThrottle* throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PendingThrottleMap::iterator it = pending_throttles_.find(
      ChildRouteIDPair(throttle->get_child_id(), throttle->get_route_id()));
  if (it != pending_throttles_.end()) {
    pending_throttles_.erase(it);
  }
}

// static
void AwResourceDispatcherHostDelegate::OnIoThreadClientReady(
    int new_child_id,
    int new_route_id) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(
          &AwResourceDispatcherHostDelegate::OnIoThreadClientReadyInternal,
          base::Unretained(
              g_webview_resource_dispatcher_host_delegate.Pointer()),
          new_child_id, new_route_id));
}

// static
void AwResourceDispatcherHostDelegate::AddPendingThrottle(
    int child_id,
    int route_id,
    IoThreadClientThrottle* pending_throttle) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(
          &AwResourceDispatcherHostDelegate::AddPendingThrottleOnIoThread,
          base::Unretained(
              g_webview_resource_dispatcher_host_delegate.Pointer()),
          child_id, route_id, pending_throttle));
}

void AwResourceDispatcherHostDelegate::AddPendingThrottleOnIoThread(
    int child_id,
    int route_id,
    IoThreadClientThrottle* pending_throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  pending_throttles_.insert(
      std::pair<ChildRouteIDPair, IoThreadClientThrottle*>(
          ChildRouteIDPair(child_id, route_id), pending_throttle));
}

void AwResourceDispatcherHostDelegate::OnIoThreadClientReadyInternal(
    int new_child_id,
    int new_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PendingThrottleMap::iterator it = pending_throttles_.find(
      ChildRouteIDPair(new_child_id, new_route_id));

  if (it != pending_throttles_.end()) {
    IoThreadClientThrottle* throttle = it->second;
    throttle->OnIoThreadClientReady(new_child_id, new_route_id);
    pending_throttles_.erase(it);
  }
}

}  // namespace android_webview
