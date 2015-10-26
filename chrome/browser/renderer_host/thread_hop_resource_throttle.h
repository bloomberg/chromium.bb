// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_THREAD_HOP_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_RENDERER_HOST_THREAD_HOP_RESOURCE_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"

namespace base {
class TimeTicks;
}

// A ResourceThrottle which defers request start, redirect, and response
// completion on an IO to UI to IO round-trip. This throttle will be added on a
// field trial to determine the cost of making some resource-related decisions
// on the UI thread. See https://crbug.com/524228.
class ThreadHopResourceThrottle : public content::ResourceThrottle {
 public:
  ThreadHopResourceThrottle();
  ~ThreadHopResourceThrottle() override;

  static bool IsEnabled();

  // content::ResourceThrottle:
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

 private:
  void ResumeAfterThreadHop();
  void Resume(const base::TimeTicks& time);

  base::WeakPtrFactory<ThreadHopResourceThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadHopResourceThrottle);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_THREAD_HOP_RESOURCE_THROTTLE_H_
