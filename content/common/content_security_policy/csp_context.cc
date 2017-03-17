// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"

namespace content {

CSPContext::CSPContext() : has_self_(false) {}

CSPContext::~CSPContext() {}

bool CSPContext::IsAllowedByCsp(CSPDirective::Name directive_name,
                                const GURL& url,
                                bool is_redirect) {
  if (SchemeShouldBypassCSP(url.scheme_piece()))
    return true;

  for (const auto& policy : policies_) {
    if (!ContentSecurityPolicy::Allow(policy, directive_name, url, this,
                                      is_redirect))
      return false;
  }
  return true;
}

void CSPContext::SetSelf(const url::Origin origin) {
  if (origin.unique()) {
    // TODO(arthursonzogni): Decide what to do with unique origins.
    has_self_ = false;
    return;
  }

  if (origin.scheme() == url::kFileScheme) {
    has_self_ = true;
    self_scheme_ = url::kFileScheme;
    self_source_ = CSPSource(url::kFileScheme, "", false, url::PORT_UNSPECIFIED,
                             false, "");
    return;
  }

  has_self_ = true;
  self_scheme_ = origin.scheme();
  self_source_ = CSPSource(
      origin.scheme(), origin.host(), false,
      origin.port() == 0 ? url::PORT_UNSPECIFIED : origin.port(),  // port
      false, "");
}

bool CSPContext::AllowSelf(const GURL& url) {
  return has_self_ && CSPSource::Allow(self_source_, url, this);
}

bool CSPContext::ProtocolMatchesSelf(const GURL& url) {
  if (!has_self_)
    return false;
  if (self_scheme_ == url::kHttpScheme)
    return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsSuborigin();
  return url.SchemeIs(self_scheme_);
}

void CSPContext::LogToConsole(const std::string& message) {
  return;
}

bool CSPContext::SchemeShouldBypassCSP(const base::StringPiece& scheme) {
  return false;
}

bool CSPContext::SelfSchemeShouldBypassCsp() {
  if (!has_self_)
    return false;
  return SchemeShouldBypassCSP(self_scheme_);
}

void CSPContext::ReportContentSecurityPolicyViolation(
    const CSPViolationParams& violation_params) {
  return;
}

CSPViolationParams::CSPViolationParams() = default;

CSPViolationParams::CSPViolationParams(
    const std::string& directive,
    const std::string& effective_directive,
    const std::string& console_message,
    const GURL& blocked_url,
    const std::vector<std::string>& report_endpoints,
    const std::string& header,
    const blink::WebContentSecurityPolicyType& disposition,
    bool after_redirect)
    : directive(directive),
      effective_directive(effective_directive),
      console_message(console_message),
      blocked_url(blocked_url),
      report_endpoints(report_endpoints),
      header(header),
      disposition(disposition),
      after_redirect(after_redirect) {}

CSPViolationParams::CSPViolationParams(const CSPViolationParams& other) =
    default;

CSPViolationParams::~CSPViolationParams() {}

}  // namespace content
