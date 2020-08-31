// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_security_policy_util.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

network::mojom::CSPSourcePtr BuildCSPSource(
    const blink::WebContentSecurityPolicySourceExpression& source) {
  return network::mojom::CSPSource::New(
      source.scheme.Utf8(),                                    // scheme
      source.host.Utf8(),                                      // host
      source.port == 0 ? url::PORT_UNSPECIFIED : source.port,  // port
      source.path.Utf8(),                                      // path
      source.is_host_wildcard == blink::kWebWildcardDispositionHasWildcard,
      source.is_port_wildcard == blink::kWebWildcardDispositionHasWildcard);
}

network::mojom::CSPSourceListPtr BuildCSPSourceList(
    const blink::WebContentSecurityPolicySourceList& source_list) {
  std::vector<network::mojom::CSPSourcePtr> sources;
  for (const auto& source : source_list.sources)
    sources.push_back(BuildCSPSource(source));

  return network::mojom::CSPSourceList::New(
      std::move(sources), source_list.allow_self, source_list.allow_star,
      source_list.allow_redirects);
}

network::mojom::ContentSecurityPolicyPtr BuildContentSecurityPolicy(
    const blink::WebContentSecurityPolicy& policy_in) {
  auto policy = network::mojom::ContentSecurityPolicy::New();

  policy->header = network::mojom::ContentSecurityPolicyHeader::New(
      policy_in.header.Utf8(), policy_in.disposition, policy_in.source);
  policy->use_reporting_api = policy_in.use_reporting_api;

  for (const auto& directive : policy_in.directives) {
    auto name = network::ToCSPDirectiveName(directive.name.Utf8());
    policy->directives[name] = BuildCSPSourceList(directive.source_list);
  }
  policy->upgrade_insecure_requests = policy_in.upgrade_insecure_requests;

  for (const blink::WebString& endpoint : policy_in.report_endpoints)
    policy->report_endpoints.push_back(endpoint.Utf8());

  return policy;
}

}  // namespace content
