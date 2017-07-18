// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/browser_url_loader_throttle.h"

#include "base/logging.h"
#include "components/safe_browsing/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"
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
    const GURL& url,
    int load_flags,
    content::ResourceType resource_type,
    bool* defer) {
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  DCHECK(!url_checker_);

  pending_checks_++;
  url_checker_ = base::MakeUnique<SafeBrowsingUrlCheckerImpl>(
      load_flags, resource_type, std::move(url_checker_delegate_),
      web_contents_getter_);
  url_checker_->CheckUrl(
      url, base::BindOnce(&BrowserURLLoaderThrottle::OnCheckUrlResult,
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
      redirect_info.new_url,
      base::BindOnce(&BrowserURLLoaderThrottle::OnCheckUrlResult,
                     base::Unretained(this)));
}

void BrowserURLLoaderThrottle::WillProcessResponse(bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  if (pending_checks_ > 0)
    *defer = true;
}

void BrowserURLLoaderThrottle::OnCheckUrlResult(bool safe) {
  if (blocked_)
    return;

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (safe) {
    if (pending_checks_ == 0) {
      // The resource load is not necessarily deferred, in that case Resume() is
      // a no-op.
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
