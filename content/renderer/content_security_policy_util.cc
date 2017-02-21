// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_security_policy_util.h"
#include "third_party/WebKit/public/platform/WebContentSecurityPolicyStruct.h"

namespace content {

CSPSource BuildCSPSource(
    const blink::WebContentSecurityPolicySourceExpression& source) {
  return CSPSource(
      source.scheme.utf8(),  // scheme
      source.host.utf8(),    // host
      source.isHostWildcard == blink::WebWildcardDispositionHasWildcard,
      source.port == 0 ? url::PORT_UNSPECIFIED : source.port,  // port
      source.isPortWildcard == blink::WebWildcardDispositionHasWildcard,
      source.path.utf8());  // path
}

CSPSourceList BuildCSPSourceList(
    const blink::WebContentSecurityPolicySourceList& source_list) {
  std::vector<CSPSource> sources;
  for (const auto& source : source_list.sources) {
    sources.push_back(BuildCSPSource(source));
  }

  return CSPSourceList(source_list.allowSelf,  // allow_self
                       source_list.allowStar,  // allow_star
                       sources);               // source_list
}

CSPDirective BuildCSPDirective(
    const blink::WebContentSecurityPolicyDirective& directive) {
  return CSPDirective(
      CSPDirective::StringToName(directive.name.utf8()),  // name
      BuildCSPSourceList(directive.sourceList));          // source_list
}

ContentSecurityPolicy BuildContentSecurityPolicy(
    const blink::WebContentSecurityPolicyPolicy& policy) {
  std::vector<CSPDirective> directives;
  for (const auto& directive : policy.directives)
    directives.push_back(BuildCSPDirective(directive));

  std::vector<std::string> report_endpoints;
  for (const blink::WebString& endpoint : policy.reportEndpoints)
    report_endpoints.push_back(endpoint.utf8());

  return ContentSecurityPolicy(policy.disposition,     // disposition
                               policy.source,          // source
                               directives,             // directives
                               report_endpoints,       // report_endpoints
                               policy.header.utf8());  // header
}

}  // namespace content
