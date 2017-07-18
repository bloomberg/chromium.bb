// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_

#include "content/common/content_export.h"

namespace content {
class NavigationHandle;

// A NavigationThrottle tracks and allows interaction with a navigation on the
// UI thread.
class CONTENT_EXPORT NavigationThrottle {
 public:
  // This is returned to the NavigationHandle to allow the navigation to
  // proceed, or to cancel it.
  enum ThrottleCheckResult {
    // The navigation proceeds uninterrupted.
    PROCEED,

    // Defers the navigation until the NavigationThrottle calls
    // NavigationHandle::Resume or NavigationHandle::CancelDeferredRequest.
    // If the NavigationHandle is destroyed while the navigation is deferred,
    // the navigation will be canceled in the network stack.
    DEFER,

    // Cancels the navigation.
    CANCEL,

    // Cancels the navigation and makes the requester of the navigation acts
    // like the request was never made.
    CANCEL_AND_IGNORE,

    // Blocks a navigation due to rules asserted before the request is made.
    // This can only be returned from WillStartRequest and also from
    // WillRedirectRequest when PlzNavigate is enabled. This will result in an
    // error page for net::ERR_BLOCKED_BY_CLIENT being loaded in the frame that
    // is navigated.
    BLOCK_REQUEST,

    // Blocks a navigation taking place in a subframe, and collapses the frame
    // owner element in the parent document (i.e. removes it from the layout).
    // This can only be returned from WillStartRequest, and also from
    // WillRedirectRequest when PlzNavigate is enabled.
    BLOCK_REQUEST_AND_COLLAPSE,

    // Blocks a navigation due to rules asserted by a response (for instance,
    // embedding restrictions like 'X-Frame-Options'). This result will only
    // be returned from WillProcessResponse.
    BLOCK_RESPONSE,
  };

  NavigationThrottle(NavigationHandle* navigation_handle);
  virtual ~NavigationThrottle();

  // Called when a network request is about to be made for this navigation.
  //
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE or DEFER and perform the destruction asynchronously.
  virtual ThrottleCheckResult WillStartRequest();

  // Called when a server redirect is received by the navigation.
  //
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE or DEFER and perform the destruction asynchronously.
  virtual ThrottleCheckResult WillRedirectRequest();

  // Called when a response's headers and metadata are available.
  //
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE, or BLOCK_RESPONSE and perform the destruction
  // asynchronously.
  virtual ThrottleCheckResult WillProcessResponse();

  // Returns the name of the throttle for logging purposes. It must not return
  // nullptr.
  virtual const char* GetNameForLogging() = 0;

  // The NavigationHandle that is tracking the information related to this
  // navigation.
  NavigationHandle* navigation_handle() const { return navigation_handle_; }

 protected:
  // Resumes a navigation that was previously deferred by this
  // NavigationThrottle.
  // Note: this may lead to the deletion of the NavigationHandle and its
  // associated NavigationThrottles, including this one.
  virtual void Resume();

  // Cancels a navigation that was previously deferred by this
  // NavigationThrottle. |result| should be equal to either:
  //  - NavigationThrottle::CANCEL,
  //  - NavigationThrottle::CANCEL_AND_IGNORE, or
  //  - NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE.
  // Note: this may lead to the deletion of the NavigationHandle and its
  // associated NavigationThrottles, including this one.
  virtual void CancelDeferredNavigation(ThrottleCheckResult result);

 private:
  NavigationHandle* navigation_handle_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_
