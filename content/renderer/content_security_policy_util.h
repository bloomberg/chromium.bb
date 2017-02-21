// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTENT_SECURITY_POLICY_UTIL_H_
#define CONTENT_RENDERER_CONTENT_SECURITY_POLICY_UTIL_H_

#include "content/common/content_security_policy/content_security_policy.h"

namespace blink {
struct WebContentSecurityPolicyPolicy;
}  // namespace blink

namespace content {

// Convert a WebContentSecurityPolicy into a ContentSecurityPolicy, these two
// classes are the reflection of each other. One in content, the other in blink.
ContentSecurityPolicy BuildContentSecurityPolicy(
    const blink::WebContentSecurityPolicyPolicy&);

}  // namespace content

#endif /* CONTENT_RENDERER_CONTENT_SECURITY_POLICY_UTIL_H_ */
