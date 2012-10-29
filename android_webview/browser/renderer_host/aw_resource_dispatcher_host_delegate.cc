// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

#include "android_webview/browser/aw_login_delegate.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/common/url_constants.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/component/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"

using navigation_interception::InterceptNavigationDelegate;

namespace {

base::LazyInstance<android_webview::AwResourceDispatcherHostDelegate>
    g_webview_resource_dispatcher_host_delegate = LAZY_INSTANCE_INITIALIZER;

// Will unconditionally cancel this resource request.
class CancelResourceThrottle : public content::ResourceThrottle {
 public:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
};

void CancelResourceThrottle::WillStartRequest(bool* defer) {
  controller()->CancelWithError(net::ERR_ACCESS_DENIED);
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

  scoped_ptr<AwContentsIoThreadClient> io_client =
      AwContentsIoThreadClient::FromID(child_id, route_id);
  DCHECK(io_client.get());

  // Part of implementation of WebSettings.allowContentAccess.
  if (request->url().SchemeIs(android_webview::kContentScheme) &&
      io_client->ShouldBlockContentUrls()) {
    throttles->push_back(new CancelResourceThrottle);
  }

  // Part of implementation of WebSettings.allowFileAccess.
  if (request->url().SchemeIsFile() && io_client->ShouldBlockFileUrls()) {
    const GURL& url = request->url();
    if (!url.has_path() ||
        // Application's assets and resources are always available.
        (url.path().find(android_webview::kAndroidResourcePath) != 0 &&
         url.path().find(android_webview::kAndroidAssetPath) != 0)) {
      throttles->push_back(new CancelResourceThrottle);
    }
  }

  // Part of implementation of WebSettings.blockNetworkLoads.
  if (io_client->ShouldBlockNetworkLoads()) {
    // Need to cancel ftp since it does not support net::LOAD_ONLY_FROM_CACHE
    // flag, so must cancel the request if network load is blocked.
    if (request->url().SchemeIs(chrome::kFtpScheme)) {
      throttles->push_back(new CancelResourceThrottle);
    } else {
      SetOnlyAllowLoadFromCache(request);
    }
  }

  // We ignore POST requests because of BUG=155250.
  if (resource_type == ResourceType::MAIN_FRAME &&
      request->method() != "POST") {
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

void AwResourceDispatcherHostDelegate::SetOnlyAllowLoadFromCache(
    net::URLRequest* request) {
  int load_flags = request->load_flags();
  load_flags &= ~(net::LOAD_BYPASS_CACHE &
                  net::LOAD_VALIDATE_CACHE &
                  net::LOAD_PREFERRING_CACHE);
  load_flags |= net::LOAD_ONLY_FROM_CACHE;
  request->set_load_flags(load_flags);
}

}
