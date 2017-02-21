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
                     const GURL& url) {
  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(directive_name, CSPDirective::DefaultSrc);
  DCHECK_NE(directive_name, CSPDirective::ChildSrc);

  std::stringstream message;

  if (policy.disposition == blink::WebContentSecurityPolicyTypeReport)
    message << "[Report Only] ";

  if (directive_name == CSPDirective::FormAction)
    message << "Refused to send form data to '";
  else if (directive_name == CSPDirective::FrameSrc)
    message << "Refused to frame '";

  message << ElideURLForReportViolation(url)
          << "' because it violates the following Content Security Policy "
             "directive: \""
          << directive.ToString() << "\".";

  if (directive.name != directive_name)
    message << " Note that '" << CSPDirective::NameToString(directive_name)
            << "' was not explicitly set, so '"
            << CSPDirective::NameToString(directive.name)
            << "' is used as a fallback.";

  message << "\n";

  context->LogToConsole(message.str());
  context->ReportViolation(CSPDirective::NameToString(directive.name),
                           CSPDirective::NameToString(directive_name),
                           message.str(), url, policy.report_endpoints,
                           policy.header, policy.disposition);
}

bool AllowDirective(CSPContext* context,
                    const ContentSecurityPolicy& policy,
                    const CSPDirective& directive,
                    CSPDirective::Name directive_name,
                    const GURL& url,
                    bool is_redirect) {
  if (CSPSourceList::Allow(directive.source_list, url, context, is_redirect))
    return true;

  ReportViolation(context, policy, directive, directive_name, url);
  return false;
}

}  // namespace

ContentSecurityPolicy::ContentSecurityPolicy()
    : disposition(blink::WebContentSecurityPolicyTypeEnforce),
      source(blink::WebContentSecurityPolicySourceHTTP) {}

ContentSecurityPolicy::ContentSecurityPolicy(
    blink::WebContentSecurityPolicyType disposition,
    blink::WebContentSecurityPolicySource source,
    const std::vector<CSPDirective>& directives,
    const std::vector<std::string>& report_endpoints,
    const std::string& header)
    : disposition(disposition),
      source(source),
      directives(directives),
      report_endpoints(report_endpoints),
      header(header) {}

ContentSecurityPolicy::ContentSecurityPolicy(const ContentSecurityPolicy&) =
    default;
ContentSecurityPolicy::~ContentSecurityPolicy() = default;

// static
bool ContentSecurityPolicy::Allow(const ContentSecurityPolicy& policy,
                                  CSPDirective::Name directive_name,
                                  const GURL& url,
                                  CSPContext* context,
                                  bool is_redirect) {
  CSPDirective::Name current_directive_name = directive_name;
  do {
    for (const CSPDirective& directive : policy.directives) {
      if (directive.name == current_directive_name) {
        bool allowed = AllowDirective(context, policy, directive,
                                      directive_name, url, is_redirect);
        return allowed ||
               policy.disposition == blink::WebContentSecurityPolicyTypeReport;
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

}  // namespace content
