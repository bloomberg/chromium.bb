// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/feature_policy/feature_policy_platform.h"

namespace content {

ParsedFeaturePolicyHeader FeaturePolicyHeaderFromWeb(
    const blink::WebParsedFeaturePolicyHeader& web_feature_policy_header) {
  ParsedFeaturePolicyHeader result;
  for (const blink::WebParsedFeaturePolicyDeclaration& web_declaration :
       web_feature_policy_header) {
    ParsedFeaturePolicyDeclaration declaration;
    declaration.feature_name = web_declaration.featureName.utf8();
    declaration.matches_all_origins = web_declaration.matchesAllOrigins;
    for (const blink::WebSecurityOrigin& web_origin : web_declaration.origins)
      declaration.origins.push_back(web_origin);
    result.push_back(declaration);
  }
  return result;
}

blink::WebParsedFeaturePolicyHeader FeaturePolicyHeaderToWeb(
    const ParsedFeaturePolicyHeader& feature_policy_header) {
  std::vector<blink::WebParsedFeaturePolicyDeclaration> result;
  for (const ParsedFeaturePolicyDeclaration& declaration :
       feature_policy_header) {
    blink::WebParsedFeaturePolicyDeclaration web_declaration;
    web_declaration.featureName =
        blink::WebString::fromUTF8(declaration.feature_name);
    web_declaration.matchesAllOrigins = declaration.matches_all_origins;
    std::vector<blink::WebSecurityOrigin> web_origins;
    for (const url::Origin& origin : declaration.origins)
      web_origins.push_back(origin);
    web_declaration.origins = web_origins;
    result.push_back(web_declaration);
  }
  return result;
}

}  // namespace content
