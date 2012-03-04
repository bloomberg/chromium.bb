// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

namespace net {
class URLRequest;
}

namespace prerender {
class PrerenderTracker;

class PrerenderResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<PrerenderResourceThrottle> {
 public:
  PrerenderResourceThrottle(PrerenderTracker* prerender_tracker,
                            const net::URLRequest* request,
                            int child_id,
                            int route_id);

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

 private:
  virtual ~PrerenderResourceThrottle();

  void ContinueRequest(bool proceed);

  PrerenderTracker* prerender_tracker_;
  const net::URLRequest* request_;
  int child_id_;
  int route_id_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderResourceThrottle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
