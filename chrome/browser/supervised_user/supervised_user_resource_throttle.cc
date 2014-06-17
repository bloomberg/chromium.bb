// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"

#include "base/bind.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

SupervisedUserResourceThrottle::SupervisedUserResourceThrottle(
    const net::URLRequest* request,
    bool is_main_frame,
    const SupervisedUserURLFilter* url_filter)
    : request_(request),
      is_main_frame_(is_main_frame),
      url_filter_(url_filter),
      weak_ptr_factory_(this) {}

SupervisedUserResourceThrottle::~SupervisedUserResourceThrottle() {}

void SupervisedUserResourceThrottle::ShowInterstitialIfNeeded(bool is_redirect,
                                                              const GURL& url,
                                                              bool* defer) {
  // Only treat main frame requests for now (ignoring subresources).
  if (!is_main_frame_)
    return;

  if (url_filter_->GetFilteringBehaviorForURL(url) !=
      SupervisedUserURLFilter::BLOCK) {
    return;
  }

  *defer = true;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&SupervisedUserNavigationObserver::OnRequestBlocked,
                 info->GetChildID(), info->GetRouteID(), url,
                 base::Bind(
                     &SupervisedUserResourceThrottle::OnInterstitialResult,
                     weak_ptr_factory_.GetWeakPtr())));
}

void SupervisedUserResourceThrottle::WillStartRequest(bool* defer) {
  ShowInterstitialIfNeeded(false, request_->url(), defer);
}

void SupervisedUserResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                         bool* defer) {
  ShowInterstitialIfNeeded(true, new_url, defer);
}

const char* SupervisedUserResourceThrottle::GetNameForLogging() const {
  return "SupervisedUserResourceThrottle";
}

void SupervisedUserResourceThrottle::OnInterstitialResult(
    bool continue_request) {
  if (continue_request)
    controller()->Resume();
  else
    controller()->Cancel();
}
