// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/base_parallel_resource_throttle.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"

namespace safe_browsing {

class BaseParallelResourceThrottle::URLLoaderThrottleHolder
    : public content::URLLoaderThrottle::Delegate {
 public:
  URLLoaderThrottleHolder(BaseParallelResourceThrottle* owner,
                          std::unique_ptr<BrowserURLLoaderThrottle> throttle)
      : owner_(owner), throttle_(std::move(throttle)) {
    throttle_->set_delegate(this);
  }
  ~URLLoaderThrottleHolder() override = default;

  BrowserURLLoaderThrottle* throttle() const { return throttle_.get(); }
  uint32_t inside_delegate_calls() const { return inside_delegate_calls_; }

  // content::URLLoaderThrottle::Delegate implementation:
  void CancelWithError(int error_code) override {
    if (!owner_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    owner_->CancelResourceLoad();
  }

  void Resume() override {
    if (!owner_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    owner_->Resume();
  }

  void Detach() {
    owner_ = nullptr;
    throttle_->set_net_event_logger(nullptr);
  }

 private:
  class ScopedDelegateCall {
   public:
    explicit ScopedDelegateCall(URLLoaderThrottleHolder* holder)
        : holder_(holder) {
      holder_->inside_delegate_calls_++;
    }
    ~ScopedDelegateCall() { holder_->inside_delegate_calls_--; }

   private:
    URLLoaderThrottleHolder* const holder_;
    DISALLOW_COPY_AND_ASSIGN(ScopedDelegateCall);
  };

  BaseParallelResourceThrottle* owner_;
  uint32_t inside_delegate_calls_ = 0;
  std::unique_ptr<BrowserURLLoaderThrottle> throttle_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderThrottleHolder);
};

BaseParallelResourceThrottle::BaseParallelResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate)
    : request_(request),
      resource_type_(resource_type),
      net_event_logger_(&request->net_log()) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  auto throttle = BrowserURLLoaderThrottle::MaybeCreate(
      std::move(url_checker_delegate), info->GetWebContentsGetterForRequest());
  throttle->set_net_event_logger(&net_event_logger_);
  url_loader_throttle_holder_ =
      std::make_unique<URLLoaderThrottleHolder>(this, std::move(throttle));
}

BaseParallelResourceThrottle::~BaseParallelResourceThrottle() {
  if (url_loader_throttle_holder_->inside_delegate_calls() > 0) {
    // The BrowserURLLoaderThrottle owned by |url_loader_throttle_holder_| is
    // calling into this object. In this case, delay destruction of
    // |url_loader_throttle_holder_|, so that the BrowserURLLoaderThrottle
    // doesn't need to worry about any delegate calls may destroy it
    // synchronously.
    url_loader_throttle_holder_->Detach();

    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, std::move(url_loader_throttle_holder_));
  }
}

void BaseParallelResourceThrottle::WillStartRequest(bool* defer) {
  content::ResourceRequest resource_request;

  net::HttpRequestHeaders full_headers;
  resource_request.headers = request_->GetFullRequestHeaders(&full_headers)
                                 ? full_headers
                                 : request_->extra_request_headers();

  resource_request.load_flags = request_->load_flags();
  resource_request.resource_type = resource_type_;

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  resource_request.has_user_gesture = info && info->HasUserGesture();

  resource_request.url = request_->url();
  resource_request.method = request_->method();

  url_loader_throttle_holder_->throttle()->WillStartRequest(resource_request,
                                                            defer);
  DCHECK(!*defer);
}

void BaseParallelResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  url_loader_throttle_holder_->throttle()->WillRedirectRequest(redirect_info,
                                                               defer);
}

void BaseParallelResourceThrottle::WillProcessResponse(bool* defer) {
  url_loader_throttle_holder_->throttle()->WillProcessResponse(
      content::ResourceResponseHead(), defer);
}

const char* BaseParallelResourceThrottle::GetNameForLogging() const {
  return "BaseParallelResourceThrottle";
}

bool BaseParallelResourceThrottle::MustProcessResponseBeforeReadingBody() {
  // The response body should not be cached before SafeBrowsing confirms that it
  // is safe to do so.
  return true;
}

void BaseParallelResourceThrottle::CancelResourceLoad() {
  Cancel();
}

}  // namespace safe_browsing
