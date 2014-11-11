// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_REFERRER_H_
#define CONTENT_PUBLIC_COMMON_REFERRER_H_

#include "base/logging.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "url/gurl.h"

namespace content {

// This struct holds a referrer URL, as well as the referrer policy to be
// applied to this URL. When passing around referrers that will eventually end
// up being used for URL requests, always use this struct.
struct CONTENT_EXPORT Referrer {
  Referrer(const GURL& url, blink::WebReferrerPolicy policy) : url(url),
                                                                policy(policy) {
  }
  Referrer() : policy(blink::WebReferrerPolicyDefault) {
  }

  GURL url;
  blink::WebReferrerPolicy policy;

  static Referrer SanitizeForRequest(const GURL& request,
                                     const Referrer& referrer) {
    Referrer sanitized_referrer(referrer.url.GetAsReferrer(), referrer.policy);

    if (!request.SchemeIsHTTPOrHTTPS() ||
        !sanitized_referrer.url.SchemeIsHTTPOrHTTPS()) {
      sanitized_referrer.url = GURL();
      return sanitized_referrer;
    }

    switch (sanitized_referrer.policy) {
      case blink::WebReferrerPolicyDefault:
        if (sanitized_referrer.url.SchemeIsSecure() &&
            !request.SchemeIsSecure()) {
          sanitized_referrer.url = GURL();
        }
        break;
      case blink::WebReferrerPolicyAlways:
        break;
      case blink::WebReferrerPolicyNever:
        sanitized_referrer.url = GURL();
        break;
      case blink::WebReferrerPolicyOrigin:
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
        break;
      default:
        NOTREACHED();
        break;
    }
    return sanitized_referrer;
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_REFERRER_H_
