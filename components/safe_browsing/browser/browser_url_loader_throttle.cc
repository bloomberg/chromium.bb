// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/browser_url_loader_throttle.h"

#include "base/logging.h"
#include "components/safe_browsing/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"
#include "components/safe_browsing/common/utils.h"
#include "content/public/common/resource_request.h"
#include "net/url_request/redirect_info.h"

namespace safe_browsing {

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::MaybeCreate(
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::Callback<content::WebContents*()>& web_contents_getter) {
  if (!url_checker_delegate ||
      !url_checker_delegate->GetDatabaseManager()->IsSupported()) {
    return nullptr;
  }

  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(std::move(url_checker_delegate),
                                   web_contents_getter));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::Callback<content::WebContents*()>& web_contents_getter)
    : url_checker_delegate_(std::move(url_checker_delegate)),
      web_contents_getter_(web_contents_getter) {}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() = default;

void BrowserURLLoaderThrottle::WillStartRequest(
    const content::ResourceRequest& request,
    bool* defer) {
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  DCHECK(!url_checker_);

  pending_checks_++;
  url_checker_ = base::MakeUnique<SafeBrowsingUrlCheckerImpl>(
      request.headers, request.load_flags, request.resource_type,
      request.has_user_gesture, std::move(url_checker_delegate_),
      web_contents_getter_);
  url_checker_->CheckUrl(
      request.url, request.method,
      base::BindOnce(&BrowserURLLoaderThrottle::OnCheckUrlResult,
                     base::Unretained(this)));
}

void BrowserURLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  pending_checks_++;
  url_checker_->CheckUrl(
      redirect_info.new_url, redirect_info.new_method,
      base::BindOnce(&BrowserURLLoaderThrottle::OnCheckUrlResult,
                     base::Unretained(this)));
}

void BrowserURLLoaderThrottle::WillProcessResponse(bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  if (pending_checks_ == 0) {
    LogDelay(base::TimeDelta());
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
}

void BrowserURLLoaderThrottle::OnCheckUrlResult(bool proceed,
                                                bool showed_interstitial) {
  if (blocked_)
    return;

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (proceed) {
    if (pending_checks_ == 0 && deferred_) {
      LogDelay(base::TimeTicks::Now() - defer_start_time_);
      deferred_ = false;
      delegate_->Resume();
    }
  } else {
    url_checker_.reset();
    blocked_ = true;
    pending_checks_ = 0;
    delegate_->CancelWithError(net::ERR_ABORTED);
  }
}

}  // namespace safe_browsing
