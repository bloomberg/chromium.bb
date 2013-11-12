// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

namespace net {
class URLRequest;
}

namespace prerender {

class PrerenderTracker;

// This class implements policy on resource requests in prerenders.  It cancels
// prerenders on certain requests.  It also defers certain requests until after
// the prerender is swapped in.
//
// TODO(davidben): Experiment with deferring network requests that
// would otherwise cancel the prerender.
class PrerenderResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<PrerenderResourceThrottle> {
 public:
  PrerenderResourceThrottle(net::URLRequest* request,
                            PrerenderTracker* tracker);

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

  // Called by the PrerenderTracker when a prerender becomes visible.
  // May only be called if currently throttling the resource.
  void Resume();

  // Called by the PrerenderTracker when a prerender is destroyed.
  // May only be called if currently throttling the resource.
  void Cancel();

 private:
  net::URLRequest* request_;
  PrerenderTracker* tracker_;
  bool throttled_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderResourceThrottle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RESOURCE_THROTTLE_H_
