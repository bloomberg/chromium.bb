// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/base_parallel_resource_throttle.h"

#include <utility>

#include "components/safe_browsing/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_request.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"

namespace safe_browsing {

void BaseParallelResourceThrottle::URLLoaderThrottleDelegateImpl::
    CancelWithError(int error_code) {
  owner_->CancelResourceLoad();
}

void BaseParallelResourceThrottle::URLLoaderThrottleDelegateImpl::Resume() {
  owner_->Resume();
}

BaseParallelResourceThrottle::BaseParallelResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate)
    : request_(request),
      resource_type_(resource_type),
      url_loader_throttle_delegate_(this),
      net_event_logger_(&request->net_log()) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  url_loader_throttle_ = BrowserURLLoaderThrottle::MaybeCreate(
      std::move(url_checker_delegate), info->GetWebContentsGetterForRequest());
  url_loader_throttle_->set_delegate(&url_loader_throttle_delegate_);
  url_loader_throttle_->set_net_event_logger(&net_event_logger_);
}

BaseParallelResourceThrottle::~BaseParallelResourceThrottle() = default;

void BaseParallelResourceThrottle::WillStartRequest(bool* defer) {
  content::ResourceRequest resource_request;

  net::HttpRequestHeaders full_headers;
  resource_request.headers = request_->GetFullRequestHeaders(&full_headers)
                                 ? full_headers.ToString()
                                 : request_->extra_request_headers().ToString();

  resource_request.load_flags = request_->load_flags();
  resource_request.resource_type = resource_type_;

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  resource_request.has_user_gesture = info && info->HasUserGesture();

  resource_request.url = request_->url();
  resource_request.method = request_->method();

  url_loader_throttle_->WillStartRequest(resource_request, defer);
  DCHECK(!*defer);
}

void BaseParallelResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  url_loader_throttle_->WillRedirectRequest(redirect_info, defer);
  DCHECK(!*defer);
}

void BaseParallelResourceThrottle::WillProcessResponse(bool* defer) {
  url_loader_throttle_->WillProcessResponse(defer);
}

const char* BaseParallelResourceThrottle::GetNameForLogging() const {
  return "BaseParallelResourceThrottle";
}

void BaseParallelResourceThrottle::CancelResourceLoad() {
  Cancel();
}

}  // namespace safe_browsing
