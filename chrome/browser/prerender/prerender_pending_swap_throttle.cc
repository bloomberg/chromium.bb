// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_pending_swap_throttle.h"

#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

namespace prerender {

PrerenderPendingSwapThrottle::PrerenderPendingSwapThrottle(
    net::URLRequest* request,
    PrerenderTracker* tracker)
    : request_(request),
      tracker_(tracker),
      throttled_(false) {
}

void PrerenderPendingSwapThrottle::WillStartRequest(bool* defer) {
  DCHECK(!throttled_);
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);

  // We only care about main frame loads.
  if (info->GetResourceType() != ResourceType::MAIN_FRAME)
    return;

  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();

  // Check if this request is for a URL we intend to swap in.
  if (!tracker_->IsPendingSwapRequestOnIOThread(child_id, route_id,
                                                  request_->url())) {
    return;
  }

  *defer = true;
  throttled_ = true;
  tracker_->AddPendingSwapThrottleOnIOThread(child_id, route_id,
                                               request_->url(),
                                               this->AsWeakPtr());
}

const char* PrerenderPendingSwapThrottle::GetNameForLogging() const {
  return "PrerenderPendingSwapThrottle";
}

void PrerenderPendingSwapThrottle::Resume() {
  DCHECK(throttled_);

  throttled_ = false;
  controller()->Resume();
}

void PrerenderPendingSwapThrottle::Cancel() {
  DCHECK(throttled_);

  throttled_ = false;
  controller()->Cancel();
}

}  // namespace prerender
