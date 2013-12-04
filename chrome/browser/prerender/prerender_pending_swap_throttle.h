// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_PENDING_SWAP_THROTTLE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_PENDING_SWAP_THROTTLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

namespace net {
class URLRequest;
}

namespace prerender {

class PrerenderTracker;

// This class implements policy on main frame navigations for prerenders
// which we would like to swap, but are pending session storage namespace
// merges. If the merge failed, we will release the throttle. If the merge
// succeeded, we will swap the prerender, making the underlying request
// obsolete.
class PrerenderPendingSwapThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<PrerenderPendingSwapThrottle> {
 public:
  PrerenderPendingSwapThrottle(net::URLRequest* request,
                               PrerenderTracker* tracker);

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

  virtual const char* GetNameForLogging() const OVERRIDE;

  // Called by the PrerenderTracker when the swap failed.
  // May only be called if currently throttling the resource.
  void Resume();

  // Called by the PrerenderTracker when the swap succeeded.
  // May only be called if currently throttling the resource.
  void Cancel();

 private:
  net::URLRequest* request_;
  PrerenderTracker* tracker_;
  bool throttled_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderPendingSwapThrottle);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_PENDING_SWAP_THROTTLE_H_
