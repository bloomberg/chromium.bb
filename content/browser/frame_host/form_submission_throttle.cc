// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/form_submission_throttle.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace content {

FormSubmissionThrottle::FormSubmissionThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

// static
std::unique_ptr<NavigationThrottle>
FormSubmissionThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsBrowserSideNavigationEnabled())
    return nullptr;

  NavigationHandleImpl* handle_impl =
      static_cast<NavigationHandleImpl*>(handle);

  if (!handle_impl->is_form_submission())
    return nullptr;

  return std::unique_ptr<NavigationThrottle>(
      new FormSubmissionThrottle(handle));
}

FormSubmissionThrottle::~FormSubmissionThrottle() {}

NavigationThrottle::ThrottleCheckResult
FormSubmissionThrottle::WillStartRequest() {
  return CheckContentSecurityPolicyFormAction(false /* is_redirect */);
}

NavigationThrottle::ThrottleCheckResult
FormSubmissionThrottle::WillRedirectRequest() {
  return CheckContentSecurityPolicyFormAction(true /* is_redirect */);
}

const char* FormSubmissionThrottle::GetNameForLogging() {
  return "FormSubmissionThrottle";
}

NavigationThrottle::ThrottleCheckResult
FormSubmissionThrottle::CheckContentSecurityPolicyFormAction(bool is_redirect) {
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle());

  if (handle->should_check_main_world_csp() == CSPDisposition::DO_NOT_CHECK)
    return NavigationThrottle::PROCEED;

  const GURL& url = handle->GetURL();
  RenderFrameHostImpl* render_frame =
      handle->frame_tree_node()->current_frame_host();

  // TODO(estark): Move this check into NavigationRequest and split it into (1)
  // check report-only CSP, (2) upgrade request if needed, (3) check enforced
  // CSP to match how frame-src works. https://crbug.com/713388
  if (render_frame->IsAllowedByCsp(CSPDirective::FormAction, url, is_redirect,
                                   handle->source_location(),
                                   CSPContext::CHECK_ALL_CSP)) {
    return NavigationThrottle::PROCEED;
  }

  return NavigationThrottle::CANCEL;
}

}  // namespace content
