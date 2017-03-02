// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include <string>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
Referrer Referrer::SanitizeForRequest(const GURL& request,
                                      const Referrer& referrer) {
  Referrer sanitized_referrer(referrer.url.GetAsReferrer(), referrer.policy);
  if (sanitized_referrer.policy == blink::WebReferrerPolicyDefault) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kReducedReferrerGranularity)) {
      sanitized_referrer.policy =
          blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      sanitized_referrer.policy =
          blink::WebReferrerPolicyNoReferrerWhenDowngrade;
    }
  }

  if (sanitized_referrer.policy < 0 ||
      sanitized_referrer.policy > blink::WebReferrerPolicyLast) {
    NOTREACHED();
    sanitized_referrer.policy = blink::WebReferrerPolicyNever;
  }

  if (!request.SchemeIsHTTPOrHTTPS() ||
      !sanitized_referrer.url.SchemeIsValidForReferrer()) {
    sanitized_referrer.url = GURL();
    return sanitized_referrer;
  }

  bool is_downgrade = sanitized_referrer.url.SchemeIsCryptographic() &&
                      !request.SchemeIsCryptographic();

  switch (sanitized_referrer.policy) {
    case blink::WebReferrerPolicyDefault:
      NOTREACHED();
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngrade:
      if (is_downgrade)
        sanitized_referrer.url = GURL();
      break;
    case blink::WebReferrerPolicyAlways:
      break;
    case blink::WebReferrerPolicyNever:
      sanitized_referrer.url = GURL();
      break;
    case blink::WebReferrerPolicyOrigin:
      sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      break;
    case blink::WebReferrerPolicyOriginWhenCrossOrigin:
      if (request.GetOrigin() != sanitized_referrer.url.GetOrigin())
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      if (is_downgrade) {
        sanitized_referrer.url = GURL();
      } else if (request.GetOrigin() != sanitized_referrer.url.GetOrigin()) {
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      }
      break;
  }
  return sanitized_referrer;
}

// static
void Referrer::SetReferrerForRequest(net::URLRequest* request,
                                     const Referrer& referrer) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!referrer.url.is_valid() ||
      command_line->HasSwitch(switches::kNoReferrers)) {
    request->SetReferrer(std::string());
  } else {
    request->SetReferrer(referrer.url.spec());
  }

  request->set_referrer_policy(ReferrerPolicyForUrlRequest(referrer));
}

// static
net::URLRequest::ReferrerPolicy Referrer::ReferrerPolicyForUrlRequest(
    const Referrer& referrer) {
  switch (referrer.policy) {
    case blink::WebReferrerPolicyAlways:
      return net::URLRequest::NEVER_CLEAR_REFERRER;
    case blink::WebReferrerPolicyNever:
      return net::URLRequest::NO_REFERRER;
    case blink::WebReferrerPolicyOrigin:
      return net::URLRequest::ORIGIN;
    case blink::WebReferrerPolicyNoReferrerWhenDowngrade:
      return net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case blink::WebReferrerPolicyOriginWhenCrossOrigin:
      return net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case blink::WebReferrerPolicyDefault:
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kReducedReferrerGranularity)) {
        return net::URLRequest::
            REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      }
      return net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      return net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

}  // namespace content
