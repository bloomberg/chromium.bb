// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FORM_SUBMISSION_THROTTLE_H_
#define CONTENT_BROWSER_FRAME_HOST_FORM_SUBMISSION_THROTTLE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;

// A FormSubmissionThrottle is responsible for enforcing the 'form-action' CSP
// directive, blocking requests which violate them.
// It is enforcing it only if PlzNavigate is enabled. Blink is still enforcing
// the 'form-action' directive on the renderer process.
class CONTENT_EXPORT FormSubmissionThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  ~FormSubmissionThrottle() override;

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;

 private:
  explicit FormSubmissionThrottle(NavigationHandle* handle);
  NavigationThrottle::ThrottleCheckResult CheckContentSecurityPolicyFormAction(
      bool is_redirect);

  DISALLOW_COPY_AND_ASSIGN(FormSubmissionThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FORM_SUBMISSION_THROTTLE_H_
