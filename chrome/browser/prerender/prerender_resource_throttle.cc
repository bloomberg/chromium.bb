// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_resource_throttle.h"

#include "base/bind.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "content/public/browser/resource_throttle_controller.h"
#include "net/url_request/url_request.h"

namespace prerender {

PrerenderResourceThrottle::PrerenderResourceThrottle(
    PrerenderTracker* prerender_tracker,
    const net::URLRequest* request,
    int child_id,
    int route_id)
    : prerender_tracker_(prerender_tracker),
      request_(request),
      child_id_(child_id),
      route_id_(route_id) {
}

PrerenderResourceThrottle::~PrerenderResourceThrottle() {
}

void PrerenderResourceThrottle::WillStartRequest(bool* defer) {
  *defer = prerender_tracker_->PotentiallyDelayRequestOnIOThread(
      request_->url(), child_id_, route_id_,
      base::Bind(&PrerenderResourceThrottle::ContinueRequest, AsWeakPtr()));
}

void PrerenderResourceThrottle::ContinueRequest(bool proceed) {
  if (proceed) {
    controller()->Resume();
  } else {
    controller()->Cancel();
  }
}

}  // namespace prerender
