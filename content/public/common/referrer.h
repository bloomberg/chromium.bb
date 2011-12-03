// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_REFERRER_H_
#define CONTENT_PUBLIC_COMMON_REFERRER_H_
#pragma once

#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebReferrerPolicy.h"

namespace content {

// This struct holds a referrer URL, as well as the referrer policy to be
// applied to this URL. When passing around referrers that will eventually end
// up being used for URL requests, always use this struct.
struct CONTENT_EXPORT Referrer {
  Referrer(const GURL& url, WebKit::WebReferrerPolicy policy) : url(url),
                                                                policy(policy) {
  }
  Referrer() : policy(WebKit::WebReferrerPolicyDefault) {
  }

  GURL url;
  WebKit::WebReferrerPolicy policy;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_REFERRER_H_
