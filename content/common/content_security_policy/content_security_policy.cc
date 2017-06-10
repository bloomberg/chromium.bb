// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/common/content_security_policy/csp_context.h"

namespace content {

namespace {

static CSPDirective::Name CSPFallback(CSPDirective::Name directive) {
  switch (directive) {
    case CSPDirective::DefaultSrc:
    case CSPDirective::FormAction:
    case CSPDirective::UpgradeInsecureRequests:
      return CSPDirective::Unknown;

    case CSPDirective::FrameSrc:
      return CSPDirective::ChildSrc;

    case CSPDirective::ChildSrc:
      return CSPDirective::DefaultSrc;

    case CSPDirective::Unknown:
      NOTREACHED();
      return CSPDirective::Unknown;
  }
  NOTREACHED();
  return CSPDirective::Unknown;
}

std::string ElideURLForReportViolation(const GURL& url) {
  // TODO(arthursonzogni): the url length should be limited to 1024 char. Find
  // a function that will not break the utf8 encoding while eliding the string.
  return url.spec();
}

void ReportViolation(CSPContext* context,
                     const ContentSecurityPolicy& policy,
                     const CSPDirective& directive,
                     const CSPDirective::Name directive_name,
                     const GURL& url,
                     bool is_redirect,
                     const SourceLocation& source_location) {
  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(directive_name, CSPDirective::DefaultSrc);
  DCHECK_NE(directive_name, CSPDirective::ChildSrc);

  // For security reasons, some urls must not be disclosed. This includes the
  // blocked url and the source location of the error. Care must be taken to
  // ensure that these are not transmitted between different cross-origin
  // renderers.
  GURL safe_url = url;
  SourceLocation safe_source_location = source_location;
  context->SanitizeDataForUseInCspViolation(is_redirect, directive_name,
                                            &safe_url, &safe_source_location);

  std::stringstream message;

  if (policy.header.type == blink::kWebContentSecurityPolicyTypeReport)
    message << "[Report Only] ";

  if (directive_name == CSPDirective::FormAction)
    message << "Refused to send form data to '";
  else if (directive_name == CSPDirective::FrameSrc)
    message << "Refused to frame '";

  message << ElideURLForReportViolation(safe_url)
          << "' because it violates the following Content Security Policy "
             "directive: \""
          << directive.ToString() << "\".";

  if (directive.name != directive_name)
    message << " Note that '" << CSPDirective::NameToString(directive_name)
            << "' was not explicitly set, so '"
            << CSPDirective::NameToString(directive.name)
            << "' is used as a fallback.";

  message << "\n";

  context->ReportContentSecurityPolicyViolation(CSPViolationParams(
      CSPDirective::NameToString(directive.name),
      CSPDirective::NameToString(directive_name), message.str(), safe_url,
      policy.report_endpoints, policy.header.header_value, policy.header.type,
      is_redirect, safe_source_location));
}

bool AllowDirective(CSPContext* context,
                    const ContentSecurityPolicy& policy,
                    const CSPDirective& directive,
                    CSPDirective::Name directive_name,
                    const GURL& url,
                    bool is_redirect,
                    const SourceLocation& source_location) {
  if (CSPSourceList::Allow(directive.source_list, url, context, is_redirect))
    return true;

  ReportViolation(context, policy, directive, directive_name, url, is_redirect,
                  source_location);
  return false;
}

const GURL ExtractInnerURL(const GURL& url) {
  if (const GURL* inner_url = url.inner_url())
    return *inner_url;
  else
    // TODO(arthursonzogni): revisit this once GURL::inner_url support blob-URL.
    return GURL(url.path());
}

bool ShouldBypassContentSecurityPolicy(CSPContext* context, const GURL& url) {
  if (url.SchemeIsFileSystem() || url.SchemeIsBlob()) {
    return context->SchemeShouldBypassCSP(ExtractInnerURL(url).scheme());
  } else {
    return context->SchemeShouldBypassCSP(url.scheme());
  }
}

}  // namespace

ContentSecurityPolicy::ContentSecurityPolicy()
    : header(std::string(),
             blink::kWebContentSecurityPolicyTypeEnforce,
             blink::kWebContentSecurityPolicySourceHTTP) {}

ContentSecurityPolicy::ContentSecurityPolicy(
    const ContentSecurityPolicyHeader& header,
    const std::vector<CSPDirective>& directives,
    const std::vector<std::string>& report_endpoints)
    : header(header),
      directives(directives),
      report_endpoints(report_endpoints) {}

ContentSecurityPolicy::ContentSecurityPolicy(const ContentSecurityPolicy&) =
    default;
ContentSecurityPolicy::~ContentSecurityPolicy() = default;

// static
bool ContentSecurityPolicy::Allow(const ContentSecurityPolicy& policy,
                                  CSPDirective::Name directive_name,
                                  const GURL& url,
                                  bool is_redirect,
                                  CSPContext* context,
                                  const SourceLocation& source_location) {
  if (ShouldBypassContentSecurityPolicy(context, url)) return true;

  CSPDirective::Name current_directive_name = directive_name;
  do {
    for (const CSPDirective& directive : policy.directives) {
      if (directive.name == current_directive_name) {
        bool allowed =
            AllowDirective(context, policy, directive, directive_name, url,
                           is_redirect, source_location);
        return allowed ||
               policy.header.type == blink::kWebContentSecurityPolicyTypeReport;
      }
    }
    current_directive_name = CSPFallback(current_directive_name);
  } while (current_directive_name != CSPDirective::Unknown);
  return true;
}

std::string ContentSecurityPolicy::ToString() const {
  std::stringstream text;
  bool is_first_policy = true;
  for (const CSPDirective& directive : directives) {
    if (!is_first_policy)
      text << "; ";
    is_first_policy = false;
    text << directive.ToString();
  }

  if (!report_endpoints.empty()) {
    if (!is_first_policy)
      text << "; ";
    is_first_policy = false;
    text << "report-uri";
    for (const std::string& endpoint : report_endpoints)
      text << " " << endpoint;
  }

  return text.str();
}

// static
bool ContentSecurityPolicy::ShouldUpgradeInsecureRequest(
    const ContentSecurityPolicy& policy) {
  for (const CSPDirective& directive : policy.directives) {
    if (directive.name == CSPDirective::UpgradeInsecureRequests)
      return true;
  }
  return false;
}

}  // namespace content
