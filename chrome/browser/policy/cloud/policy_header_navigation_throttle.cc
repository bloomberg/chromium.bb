// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/policy_header_navigation_throttle.h"

#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace policy {

namespace {
void AddHeader(content::NavigationHandle* handle) {
  content::BrowserContext* context =
      handle->GetWebContents()->GetBrowserContext();

  if (context->IsOffTheRecord())
    return;

  PolicyHeaderService* policy_header_service =
      PolicyHeaderServiceFactory::GetForBrowserContext(context);
  if (!policy_header_service)
    return;

  policy_header_service->AddPolicyHeaders(
      handle->GetURL(),
      base::BindOnce(&content::NavigationHandle::SetRequestHeader,
                     base::Unretained(handle)));
}
}  // namespace

PolicyHeaderNavigationThrottle::~PolicyHeaderNavigationThrottle() = default;

PolicyHeaderNavigationThrottle::PolicyHeaderNavigationThrottle(
    content::NavigationHandle* handle)
    : NavigationThrottle(handle) {}

content::NavigationThrottle::ThrottleCheckResult
PolicyHeaderNavigationThrottle::WillStartRequest() {
  AddHeader(navigation_handle());
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PolicyHeaderNavigationThrottle::WillRedirectRequest() {
  AddHeader(navigation_handle());
  return content::NavigationThrottle::PROCEED;
}

const char* PolicyHeaderNavigationThrottle::GetNameForLogging() {
  return "PolicyHeaderNavigationThrottle";
}

}  // namespace policy
