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

CSPContext::CSPContext() : has_self_(false) {}

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

bool CSPContext::ProtocolIsSelf(const GURL& url) {
  if (!has_self_)
    return false;
  return url.SchemeIs(self_scheme_);
}

const std::string& CSPContext::GetSelfScheme() {
  return self_scheme_;
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
    bool after_redirect,
    const SourceLocation& source_location)
    : directive(directive),
      effective_directive(effective_directive),
      console_message(console_message),
      blocked_url(blocked_url),
      report_endpoints(report_endpoints),
      header(header),
      disposition(disposition),
      after_redirect(after_redirect),
      source_location(source_location) {}

CSPViolationParams::CSPViolationParams(const CSPViolationParams& other) =
    default;

CSPViolationParams::~CSPViolationParams() {}

}  // namespace content
