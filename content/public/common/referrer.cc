// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include <string>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "net/url_request/url_request.h"

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

  if (!request.SchemeIsHTTPOrHTTPS() ||
      !sanitized_referrer.url.SchemeIsValidForReferrer()) {
    sanitized_referrer.url = GURL();
    return sanitized_referrer;
  }

  bool is_downgrade = sanitized_referrer.url.SchemeIsCryptographic() &&
                      !request.SchemeIsCryptographic();

  if (sanitized_referrer.policy < 0 ||
      sanitized_referrer.policy > blink::WebReferrerPolicyLast) {
    NOTREACHED();
    sanitized_referrer.policy = blink::WebReferrerPolicyNever;
  }

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

  net::URLRequest::ReferrerPolicy net_referrer_policy =
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  switch (referrer.policy) {
    case blink::WebReferrerPolicyAlways:
      net_referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
      break;
    case blink::WebReferrerPolicyNever:
      net_referrer_policy = net::URLRequest::NO_REFERRER;
      break;
    case blink::WebReferrerPolicyOrigin:
      net_referrer_policy = net::URLRequest::ORIGIN;
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngrade:
      net_referrer_policy =
          net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;
    case blink::WebReferrerPolicyOriginWhenCrossOrigin:
      net_referrer_policy =
          net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      break;
    case blink::WebReferrerPolicyDefault:
      net_referrer_policy =
          command_line->HasSwitch(switches::kReducedReferrerGranularity)
              ? net::URLRequest::
                    REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN
              : net::URLRequest::
                    CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      net_referrer_policy = net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      break;
  }
  request->set_referrer_policy(net_referrer_policy);
}

}  // namespace content
