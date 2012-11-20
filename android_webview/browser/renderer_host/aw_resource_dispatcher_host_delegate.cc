// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

#include "android_webview/browser/aw_login_delegate.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/common/url_constants.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"

using content::InterceptNavigationDelegate;

namespace {

using android_webview::AwContentsIoThreadClient;

base::LazyInstance<android_webview::AwResourceDispatcherHostDelegate>
    g_webview_resource_dispatcher_host_delegate = LAZY_INSTANCE_INITIALIZER;

void SetOnlyAllowLoadFromCache(
    net::URLRequest* request) {
  int load_flags = request->load_flags();
  load_flags &= ~(net::LOAD_BYPASS_CACHE &
                  net::LOAD_VALIDATE_CACHE &
                  net::LOAD_PREFERRING_CACHE);
  load_flags |= net::LOAD_ONLY_FROM_CACHE;
  request->set_load_flags(load_flags);
}

// May cancel this resource request based on result of Java callbacks.
class MaybeCancelResourceThrottle : public content::ResourceThrottle {
 public:
  MaybeCancelResourceThrottle(int child_id,
                              int route_id,
                              net::URLRequest* request)
      : child_id_(child_id),
        route_id_(route_id),
        request_(request) { }
  virtual void WillStartRequest(bool* defer) OVERRIDE;

  scoped_ptr<AwContentsIoThreadClient> GetIoThreadClient() {
    return AwContentsIoThreadClient::FromID(child_id_, route_id_);
  }

 private:
  int child_id_;
  int route_id_;
  net::URLRequest* request_;
};

void MaybeCancelResourceThrottle::WillStartRequest(bool* defer) {
  // If there is no IO thread client set at this point, use a
  // restrictive policy. This can happen for blocked popup
  // windows for example.
  // TODO(benm): Revert this to a DCHECK when the we support
  // pop up windows being created in the WebView, as at that
  // time we should always have an IoThreadClient at this
  // point (i.e., the one associated with the new popup).
  if (!GetIoThreadClient()) {
    controller()->CancelWithError(net::ERR_ACCESS_DENIED);
    return;
  }

  // Part of implementation of WebSettings.allowContentAccess.
  if (request_->url().SchemeIs(android_webview::kContentScheme) &&
      GetIoThreadClient()->ShouldBlockContentUrls()) {
    controller()->CancelWithError(net::ERR_ACCESS_DENIED);
    return;
  }

  // Part of implementation of WebSettings.allowFileAccess.
  if (request_->url().SchemeIsFile() &&
      GetIoThreadClient()->ShouldBlockFileUrls()) {
    const GURL& url = request_->url();
    if (!url.has_path() ||
        // Application's assets and resources are always available.
        (url.path().find(android_webview::kAndroidResourcePath) != 0 &&
         url.path().find(android_webview::kAndroidAssetPath) != 0)) {
      controller()->CancelWithError(net::ERR_ACCESS_DENIED);
      return;
    }
  }

  if (GetIoThreadClient()->ShouldBlockNetworkLoads()) {
    if (request_->url().SchemeIs(chrome::kFtpScheme)) {
      controller()->CancelWithError(net::ERR_ACCESS_DENIED);
      return;
    }
    SetOnlyAllowLoadFromCache(request_);
  }
}

}  // namespace

namespace android_webview {

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

  throttles->push_back(new MaybeCancelResourceThrottle(
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

}
