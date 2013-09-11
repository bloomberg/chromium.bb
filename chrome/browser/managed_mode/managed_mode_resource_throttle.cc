// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

ManagedModeResourceThrottle::ManagedModeResourceThrottle(
    const net::URLRequest* request,
    bool is_main_frame,
    const ManagedModeURLFilter* url_filter)
    : weak_ptr_factory_(this),
      request_(request),
      is_main_frame_(is_main_frame),
      url_filter_(url_filter) {}

ManagedModeResourceThrottle::~ManagedModeResourceThrottle() {}

void ManagedModeResourceThrottle::ShowInterstitialIfNeeded(bool is_redirect,
                                                           const GURL& url,
                                                           bool* defer) {
  // Only treat main frame requests for now (ignoring subresources).
  if (!is_main_frame_)
    return;

  if (url_filter_->GetFilteringBehaviorForURL(url) !=
      ManagedModeURLFilter::BLOCK) {
    return;
  }

  *defer = true;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ManagedModeNavigationObserver::OnRequestBlocked,
                 info->GetChildID(), info->GetRouteID(), url,
                 base::Bind(&ManagedModeResourceThrottle::OnInterstitialResult,
                            weak_ptr_factory_.GetWeakPtr())));
}

void ManagedModeResourceThrottle::WillStartRequest(bool* defer) {
  ShowInterstitialIfNeeded(false, request_->url(), defer);
}

void ManagedModeResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                      bool* defer) {
  ShowInterstitialIfNeeded(true, new_url, defer);
}

void ManagedModeResourceThrottle::OnInterstitialResult(bool continue_request) {
  if (continue_request)
    controller()->Resume();
  else
    controller()->Cancel();
}
