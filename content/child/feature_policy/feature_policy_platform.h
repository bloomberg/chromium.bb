// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FEATURE_POLICY_FEATURE_POLICY_PLATFORM_H_
#define CONTENT_CHILD_FEATURE_POLICY_FEATURE_POLICY_PLATFORM_H_

#include "content/common/feature_policy/feature_policy.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"

namespace content {

// Conversions between ParsedFeaturePolicyHeader and
// WebParsedFeaturePolicyHeader
ParsedFeaturePolicyHeader FeaturePolicyHeaderFromWeb(
    const blink::WebParsedFeaturePolicyHeader& web_feature_policy_header);
blink::WebParsedFeaturePolicyHeader FeaturePolicyHeaderToWeb(
    const ParsedFeaturePolicyHeader& feature_policy_header);

}  // namespace content

#endif  // CONTENT_CHILD_FEATURE_POLICY_FEATURE_POLICY_PLATFORM_H_
