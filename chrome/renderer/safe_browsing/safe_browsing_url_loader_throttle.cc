// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/safe_browsing_url_loader_throttle.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/url_request/redirect_info.h"

namespace safe_browsing {

SafeBrowsingURLLoaderThrottle::SafeBrowsingURLLoaderThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id)
    : safe_browsing_(safe_browsing),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {}

SafeBrowsingURLLoaderThrottle::~SafeBrowsingURLLoaderThrottle() = default;

void SafeBrowsingURLLoaderThrottle::WillStartRequest(
    const GURL& url,
    int load_flags,
    content::ResourceType resource_type,
    bool* defer) {
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  DCHECK(!url_checker_);

  pending_checks_++;
  // Use a weak pointer to self because |safe_browsing_| is not owned by this
  // object.
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id_, mojo::MakeRequest(&url_checker_), url, load_flags,
      resource_type,
      base::BindOnce(&SafeBrowsingURLLoaderThrottle::OnCheckUrlResult,
                     weak_factory_.GetWeakPtr()));
  safe_browsing_ = nullptr;

  url_checker_.set_connection_error_handler(
      base::Bind(&SafeBrowsingURLLoaderThrottle::OnConnectionError,
                 base::Unretained(this)));
}

void SafeBrowsingURLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  if (!url_checker_) {
    DCHECK_EQ(0u, pending_checks_);
    return;
  }

  pending_checks_++;
  url_checker_->CheckUrl(
      redirect_info.new_url,
      base::BindOnce(&SafeBrowsingURLLoaderThrottle::OnCheckUrlResult,
                     base::Unretained(this)));
}

void SafeBrowsingURLLoaderThrottle::WillProcessResponse(bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  if (pending_checks_ > 0)
    *defer = true;
}

void SafeBrowsingURLLoaderThrottle::OnCheckUrlResult(bool safe) {
  if (blocked_ || !url_checker_)
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

void SafeBrowsingURLLoaderThrottle::OnConnectionError() {
  DCHECK(!blocked_);

  // If a service-side disconnect happens, treat all URLs as if they are safe.
  url_checker_.reset();
  pending_checks_ = 0;
  delegate_->Resume();
}

}  // namespace safe_browsing
