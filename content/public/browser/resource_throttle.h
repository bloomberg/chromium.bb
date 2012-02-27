// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_

#include <vector>

class GURL;

namespace content {

class ResourceThrottleController;

// A ResourceThrottle gets notified at various points during the process of
// loading a resource.  At each stage, it has the opportunity to defer the
// resource load.  The ResourceThrottleController interface may be used to
// resume a deferred resource load, or it may be used to cancel a resource
// load at any time.
class ResourceThrottle {
 public:
  virtual ~ResourceThrottle() {}

  virtual void WillStartRequest(bool* defer) {}
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) {}
  virtual void WillProcessResponse(bool* defer) {}

  void set_controller_for_testing(ResourceThrottleController* c) {
    controller_ = c;
  }

 protected:
  ResourceThrottle() : controller_(NULL) {}
  ResourceThrottleController* controller() { return controller_; }

 private:
  friend class ThrottlingResourceHandler;
  void set_controller(ResourceThrottleController* c) { controller_ = c; }

  ResourceThrottleController* controller_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_THROTTLE_H_
