// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"

namespace content {

namespace {

// Helper function that returns true if |policy| should be checked under
// |check_csp_disposition|.
bool ShouldCheckPolicy(const ContentSecurityPolicy& policy,
                       CSPContext::CheckCSPDisposition check_csp_disposition) {
  switch (check_csp_disposition) {
    case CSPContext::CHECK_REPORT_ONLY_CSP:
      return policy.header.type == blink::kWebContentSecurityPolicyTypeReport;
    case CSPContext::CHECK_ENFORCED_CSP:
      return policy.header.type == blink::kWebContentSecurityPolicyTypeEnforce;
    case CSPContext::CHECK_ALL_CSP:
      return true;
  }
  NOTREACHED();
  return true;
}

}  // namespace

CSPContext::CSPContext() {}
CSPContext::~CSPContext() {}

bool CSPContext::IsAllowedByCsp(CSPDirective::Name directive_name,
                                const GURL& url,
                                bool is_redirect,
                                const SourceLocation& source_location,
                                CheckCSPDisposition check_csp_disposition) {
  if (SchemeShouldBypassCSP(url.scheme_piece()))
    return true;

  bool allow = true;
  for (const auto& policy : policies_) {
    if (ShouldCheckPolicy(policy, check_csp_disposition)) {
      allow &= ContentSecurityPolicy::Allow(policy, directive_name, url,
                                            is_redirect, this, source_location);
    }
  }
  return allow;
}

bool CSPContext::ShouldModifyRequestUrlForCsp(
    const GURL& url,
    bool is_subresource_or_form_submission,
    GURL* new_url) {
  for (const auto& policy : policies_) {
    if (url.scheme() == "http" &&
        ContentSecurityPolicy::ShouldUpgradeInsecureRequest(policy) &&
        is_subresource_or_form_submission) {
      *new_url = url;
      GURL::Replacements replacements;
      replacements.SetSchemeStr("https");
      if (url.port() == "80")
        replacements.SetPortStr("443");
      *new_url = new_url->ReplaceComponents(replacements);
      return true;
    }
  }
  return false;
}

void CSPContext::SetSelf(const url::Origin origin) {
  self_source_.reset();

  // When the origin is unique, no URL should match with 'self'. That's why
  // |self_source_| stays undefined here.
  if (origin.unique())
    return;

  if (origin.scheme() == url::kFileScheme) {
    self_source_ = CSPSource(url::kFileScheme, "", false, url::PORT_UNSPECIFIED,
                             false, "");
    return;
  }

  self_source_ = CSPSource(
      origin.scheme(), origin.host(), false,
      origin.port() == 0 ? url::PORT_UNSPECIFIED : origin.port(), false, "");

  DCHECK_NE("", self_source_->scheme);
}

bool CSPContext::SchemeShouldBypassCSP(const base::StringPiece& scheme) {
  return false;
}

void CSPContext::SanitizeDataForUseInCspViolation(
    bool is_redirect,
    CSPDirective::Name directive,
    GURL* blocked_url,
    SourceLocation* source_location) const {
  return;
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
    bool use_reporting_api,
    const std::string& header,
    const blink::WebContentSecurityPolicyType& disposition,
    bool after_redirect,
    const SourceLocation& source_location)
    : directive(directive),
      effective_directive(effective_directive),
      console_message(console_message),
      blocked_url(blocked_url),
      report_endpoints(report_endpoints),
      use_reporting_api(use_reporting_api),
      header(header),
      disposition(disposition),
      after_redirect(after_redirect),
      source_location(source_location) {}

CSPViolationParams::CSPViolationParams(const CSPViolationParams& other) =
    default;
CSPViolationParams::~CSPViolationParams() {}

}  // namespace content
