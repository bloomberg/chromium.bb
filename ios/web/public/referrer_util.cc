// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/referrer_util.h"

#include "base/logging.h"
#include "ios/web/public/referrer.h"
#include "url/gurl.h"

namespace web {

GURL ReferrerForHeader(const GURL& referrer) {
  DCHECK(referrer.is_valid());
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return referrer.ReplaceComponents(replacements);
}

std::string ReferrerHeaderValueForNavigation(
    const GURL& destination,
    const web::Referrer& referrer) {
  std::string referrer_value;
  bool leaving_secure_scheme =
      referrer.url.SchemeIsSecure() && !destination.SchemeIsSecure();
  if (referrer.policy == ReferrerPolicyAlways ||
      (referrer.policy == ReferrerPolicyDefault && !leaving_secure_scheme)) {
    if (referrer.url.is_valid())
      referrer_value = ReferrerForHeader(referrer.url).spec();
  } else if (referrer.policy == ReferrerPolicyOrigin) {
    referrer_value = referrer.url.GetOrigin().spec();
  } else {
    // Policy is Never, or it's Default with a secure->insecure transition, so
    // leave it empty.
  }
  return referrer_value;
}

net::URLRequest::ReferrerPolicy PolicyForNavigation(
    const GURL& destination,
    const web::Referrer& referrer) {
  net::URLRequest::ReferrerPolicy net_referrer_policy =
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  if (referrer.url.is_valid()) {
    GURL referrer_url(ReferrerHeaderValueForNavigation(destination, referrer));
    if (!referrer_url.is_empty()) {
      switch (referrer.policy) {
        case ReferrerPolicyDefault:
          break;
        case ReferrerPolicyAlways:
        case ReferrerPolicyNever:
        case ReferrerPolicyOrigin:
          net_referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
          break;
        default:
          NOTREACHED();
      }
    }
  }
  return net_referrer_policy;
}

ReferrerPolicy ReferrerPolicyFromString(const std::string& policy) {
  if (policy == "never")
    return ReferrerPolicyNever;
  if (policy == "always")
    return ReferrerPolicyAlways;
  if (policy == "origin")
    return ReferrerPolicyOrigin;
  return web::ReferrerPolicyDefault;
}

}  // namespace web
